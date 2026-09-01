// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilityTask_RotateToTarget.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Tags/ZZZGameplayTags.h"

UAbilityTask_RotateToTarget* UAbilityTask_RotateToTarget::RotateToTarget(
	UGameplayAbility* OwningAbility,
	float InSearchRadius,
	float InInterpSpeed)
{
	UAbilityTask_RotateToTarget* Task = NewAbilityTask<UAbilityTask_RotateToTarget>(OwningAbility);
	Task->SearchRadius = FMath::Max(0.0f, InSearchRadius);
	Task->InterpSpeed = FMath::Max(0.0f, InInterpSpeed);
	return Task;
}

void UAbilityTask_RotateToTarget::Activate()
{
	Super::Activate();

	TargetActor = FindNearestTarget();

	if (TargetActor)
	{
		bTargetFound = true;
		bTickingTask = true;
		OnTargetFound.Broadcast();
	}
	else
	{
		bTargetFound = false;
		OnNoTargetFound.Broadcast();
		// Still end cleanly — we don't want to block the ability.
		EndTask();
	}
}

void UAbilityTask_RotateToTarget::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	if (!IsValid(TargetActor))
	{
		bTickingTask = false;
		return;
	}

	AActor* Avatar = GetAvatarActor();
	if (!Avatar)
	{
		return;
	}

	// Pass-through gate (2026-08-29): while the owner carries State.PassThrough
	// (granted by AnimNotifyState_CollisionPassThrough's WindowTag — a dash
	// passing THROUGH an enemy), stop steering toward the target; the root
	// motion drives the facing. Skip the frame only — resume when the window
	// closes (NotifyEnd removes the tag).
	if (UAbilitySystemComponent* ASC = Avatar->GetComponentByClass<UAbilitySystemComponent>())
	{
		if (ASC->HasMatchingGameplayTag(FZZZGameplayTags::Get().State_PassThrough))
		{
			return;
		}
	}

	// Compute target direction (yaw only — ignore pitch/roll)
	const FVector AvatarLocation = Avatar->GetActorLocation();
	const FVector TargetLocation = TargetActor->GetActorLocation();
	const FVector Direction = (TargetLocation - AvatarLocation).GetSafeNormal2D();

	if (Direction.IsNearlyZero())
	{
		return;
	}

	const FRotator TargetRotation = Direction.Rotation();
	const FRotator CurrentRotation = Avatar->GetActorRotation();

	// Interpolate yaw only
	const float NewYaw = UKismetMathLibrary::RInterpTo(
		CurrentRotation,
		TargetRotation,
		DeltaTime,
		InterpSpeed).Yaw;

	Avatar->SetActorRotation(FRotator(CurrentRotation.Pitch, NewYaw, CurrentRotation.Roll));
}

void UAbilityTask_RotateToTarget::OnDestroy(bool bInOwnerFinished)
{
	bTickingTask = false;
	TargetActor = nullptr;

	Super::OnDestroy(bInOwnerFinished);
}

AActor* UAbilityTask_RotateToTarget::FindNearestTarget() const
{
	AActor* Avatar = GetAvatarActor();
	if (!Avatar)
	{
		return nullptr;
	}

	UWorld* World = Avatar->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const FVector Origin = Avatar->GetActorLocation();
	const FCollisionShape Sphere = FCollisionShape::MakeSphere(SearchRadius);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Avatar);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(
		Overlaps,
		Origin,
		FQuat::Identity,
		FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllDynamicObjects),
		Sphere,
		QueryParams);

	AActor* Nearest = nullptr;
	float NearestDistSq = TNumericLimits<float>::Max();

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* HitActor = Overlap.GetActor();
		if (!HitActor || HitActor == Avatar)
		{
			continue;
		}

		// Skip inactive squad members: they are hidden (not destroyed) while
		// another member is on the field, and must not be auto-targeted.
		if (HitActor->IsHidden())
		{
			continue;
		}

		// Only target actors that have an ASC (enemies, not props)
		if (!Cast<IAbilitySystemInterface>(HitActor))
		{
			continue;
		}

		// Skip dead targets
		UAbilitySystemComponent* ASC = Cast<IAbilitySystemInterface>(HitActor)->GetAbilitySystemComponent();
		if (ASC && ASC->HasMatchingGameplayTag(FZZZGameplayTags::Get().State_Dead))
		{
			continue;
		}

		// 阵营对立过滤 (2026-08-31): 索敌只锁敌对阵营 — 玩家攻击只锁敌人,
		// 敌人攻击只锁玩家。切换进出场期间同场的队友(退场中可见+碰撞开,
		// IsHidden 兜底拦不住)绝不能成为索敌目标, 否则攻击会锁到队友位置。
		// 攻击者无阵营 tag(非 ZZZ 角色)时跳过此过滤, 保持旧行为。
		{
			UAbilitySystemComponent* AvatarASC =
				Cast<IAbilitySystemInterface>(Avatar)->GetAbilitySystemComponent();
			const FZZZGameplayTags& Tags = FZZZGameplayTags::Get();
			const bool bAttackerIsPlayer = AvatarASC && AvatarASC->HasMatchingGameplayTag(Tags.State_Player);
			const bool bAttackerIsEnemy = AvatarASC && AvatarASC->HasMatchingGameplayTag(Tags.State_Enemy);
			if (bAttackerIsPlayer || bAttackerIsEnemy)
			{
				const bool bTargetIsEnemy = ASC && ASC->HasMatchingGameplayTag(Tags.State_Enemy);
				const bool bTargetIsPlayer = ASC && ASC->HasMatchingGameplayTag(Tags.State_Player);
				if (!((bAttackerIsPlayer && bTargetIsEnemy) || (bAttackerIsEnemy && bTargetIsPlayer)))
				{
					continue;
				}
			}
		}

		const float DistSq = FVector::DistSquared(Origin, HitActor->GetActorLocation());
		if (DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = HitActor;
		}
	}

	return Nearest;
}
