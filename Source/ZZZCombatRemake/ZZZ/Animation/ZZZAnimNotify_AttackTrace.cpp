// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZAnimNotify_AttackTrace.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Effects/ZZZStatusGameplayEffects.h"
#include "GameplayEffect.h"
#include "Tags/ZZZGameplayTags.h"
#include "UObject/ConstructorHelpers.h"
#include "ZZZCombatRemake.h"  // LogZZZCombatRemake

UZZZAnimNotify_AttackTrace::UZZZAnimNotify_AttackTrace()
{
	// Default assets — per-instance overridable in the montage editor.
	HitStopEffect = UZZZGameplayEffect_HitStop::StaticClass();

	// Damage GE default = the shared GE_Damage BP (/Game/ZZZ/GE/GE_Damage).
	// FClassFinder in the CDO ctor is safe here: the CDO is lazily created
	// after module load, when this module's native classes (UZZZDamageExecution
	// among them — GE_Damage's ExecCalc) are all registered. Instances with
	// per-hit damage setups still override the slot per notify.
	static ConstructorHelpers::FClassFinder<UGameplayEffect> DamageFinder(
		TEXT("/Game/ZZZ/GE/GE_Damage"));
	if (DamageFinder.Succeeded())
	{
		DamageEffect = DamageFinder.Class;
	}

	// Low shake by default. Same CDO-timing caveat as the cue classes: the
	// notify CDO can be constructed before InitializeNativeGameplayTags runs,
	// so request the ini-registered tag directly as a fallback (ini tags are
	// up before our module loads — the fallback is deterministic on cold load).
	CameraShakeCueTag = FZZZGameplayTags::Get().GameplayCue_ZZZ_CameraShakeLow;
	if (!CameraShakeCueTag.IsValid())
	{
		CameraShakeCueTag = FGameplayTag::RequestGameplayTag(
			FName("GameplayCue.ZZZ.CameraShake.Low"), false);
	}
}

void UZZZAnimNotify_AttackTrace::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (!MeshComp || !DamageEffect)
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		return;
	}

	// Get attacker's ASC
	IAbilitySystemInterface* AttackerGAS = Cast<IAbilitySystemInterface>(Owner);
	if (!AttackerGAS)
	{
		return;
	}

	UAbilitySystemComponent* AttackerASC = AttackerGAS->GetAbilitySystemComponent();
	if (!AttackerASC)
	{
		return;
	}

	// Sphere trace from the bone socket forward
	const FVector Start = MeshComp->GetSocketLocation(DamageSourceBone);
	const FVector Forward = Owner->GetActorForwardVector();
	const FVector End = Start + Forward * TraceDistance;

	FCollisionShape Sphere = FCollisionShape::MakeSphere(TraceRadius);
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Owner);

	const FZZZGameplayTags& Tags = FZZZGameplayTags::Get();

	// 阵营感知扫描通道 (2026-08-31, 第二次修正): 扫"攻击者自己的阵营通道"。
	// UE 查询语义: 扫通道 X 命中物体 Y ⟺ Y 对 X 响应 Block。各 profile:
	//   玩家 capsule: PlayerCapsule=Ignore, EnemyCapsule=Block
	//   敌人 capsule: PlayerCapsule=Block,  EnemyCapsule=Ignore
	// 所以玩家挥击扫 PlayerCapsule → 只命中敌人(Block), 不命中队友(Ignore);
	// 敌人挥击扫 EnemyCapsule → 只命中玩家。第一次实现误写为"对方通道"
	// (玩家扫 EnemyCapsule): 玩家对 EnemyCapsule=Block 反而不命中敌人,
	// 敌人扫 PlayerCapsule 时玩家对 PlayerCapsule=Ignore → 敌人永远打不到玩家。
	// (CollisionPassThrough 方向与此对称相反 — 它翻转"对方通道"以穿过, 是对的。)
	ECollisionChannel SweepChannel = ECC_Pawn;
	if (AttackerASC->HasMatchingGameplayTag(Tags.State_Player))
	{
		SweepChannel = ECC_GameTraceChannel1; // PlayerCapsule
	}
	else if (AttackerASC->HasMatchingGameplayTag(Tags.State_Enemy))
	{
		SweepChannel = ECC_GameTraceChannel2; // EnemyCapsule
	}

	TArray<FHitResult> Hits;
	Owner->GetWorld()->SweepMultiByChannel(Hits, Start, End, FQuat::Identity,
		SweepChannel, Sphere, QueryParams);

	if (Hits.Num() == 0)
	{
		return;
	}

	// 一次 sweep 可能对同一 actor 返回多个 hit(Chaos 组合 shape / 多碰撞体),
	// 且本帧可能有多个 AttackTrace notify 实例同时触发 — 每个目标每帧最多
	// 结算一次伤害。否则日志会出现同一时刻多条同值伤害(2026-08-31 修复)。
	UE_LOG(LogZZZCombatRemake, Log, TEXT("[AttackTrace] sweep hits=%d"), Hits.Num());
	TArray<AActor*> ProcessedActors;

	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();
		if (!HitActor || HitActor == Owner || ProcessedActors.Contains(HitActor))
		{
			continue;
		}
		ProcessedActors.Add(HitActor);

		IAbilitySystemInterface* TargetGAS = Cast<IAbilitySystemInterface>(HitActor);
		if (!TargetGAS)
		{
			continue;
		}

		UAbilitySystemComponent* TargetASC = TargetGAS->GetAbilitySystemComponent();
		if (!TargetASC)
		{
			continue;
		}

		// Pre-hit snapshot — read the target's state BEFORE applying the damage
		// GE: a killing blow makes the AttributeSet synchronously grant
		// State.Dead, so reading afterwards would wrongly swallow the killing
		// frame's feedback (which is exactly the frame that should hit hardest).
		const bool bAlive = !TargetASC->HasMatchingGameplayTag(Tags.State_Dead);
		const bool bVulnerable = !TargetASC->HasMatchingGameplayTag(Tags.State_Invulnerable);

		FGameplayEffectContextHandle Context = AttackerASC->MakeEffectContext();
		Context.AddOrigin(Hit.ImpactPoint);

		FGameplayEffectSpecHandle Spec = AttackerASC->MakeOutgoingSpec(DamageEffect, 1, Context);
		if (Spec.IsValid())
		{
			// Pass per-hit damage values via SetByCaller → ExecCalc reads them
			Spec.Data->SetSetByCallerMagnitude(Tags.Data_Damage, BaseDamage);
			Spec.Data->SetSetByCallerMagnitude(Tags.Data_Daze, DazeBuildup);

			AttackerASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
		}

		// Hit feedback (打击感, Phase 3 Task 5a): fires only when the hit
		// actually landed — this loop only runs for actors swept by the trace
		// (a whiffed swing never reaches here), and the pre-hit gates skip
		// invulnerable (perfect-dodge absorb) and dead targets.
		if (!(bAlive && bVulnerable))
		{
			continue;
		}

		if (bApplyHitStop && HitStopEffect)
		{
			// Bidirectional freeze: attacker and target each get their own
			// spec (a single spec must not be applied twice). The GE expires
			// on world time and the bridge restores CustomTimeDilation from
			// the recomputed attribute — no timers, no guards.
			FGameplayEffectSpecHandle SelfHitStopSpec = AttackerASC->MakeOutgoingSpec(
				HitStopEffect, 1, AttackerASC->MakeEffectContext());
			if (SelfHitStopSpec.IsValid())
			{
				AttackerASC->ApplyGameplayEffectSpecToSelf(*SelfHitStopSpec.Data.Get());
			}

			FGameplayEffectSpecHandle TargetHitStopSpec = AttackerASC->MakeOutgoingSpec(
				HitStopEffect, 1, AttackerASC->MakeEffectContext());
			if (TargetHitStopSpec.IsValid())
			{
				AttackerASC->ApplyGameplayEffectSpecToTarget(*TargetHitStopSpec.Data.Get(), TargetASC);
			}
		}

		if (bApplyCameraShake)
		{
			// Empty tag falls back to Low (the class default could be lost if
			// the CDO was constructed before tag registration on a cold load).
			const FGameplayTag CueTag = CameraShakeCueTag.IsValid()
				? CameraShakeCueTag
				: Tags.GameplayCue_ZZZ_CameraShakeLow;
			if (CueTag.IsValid())
			{
				FGameplayCueParameters ShakeParams;
				ShakeParams.Instigator = Owner;
				ShakeParams.RawMagnitude = BaseDamage;  // reserved for intensity scaling
				TargetASC->ExecuteGameplayCue(CueTag, ShakeParams);
			}
		}
	}
}
