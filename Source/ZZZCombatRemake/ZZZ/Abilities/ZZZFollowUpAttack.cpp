// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZFollowUpAttack.h"
#include "AbilitySystemComponent.h"
#include "AbilityTask_RotateToTarget.h"
#include "Enemies/ZZZCombatEnemy.h"
#include "MotionWarpingComponent.h"
#include "Tags/ZZZGameplayTags.h"
#include "ZZZBasicAttack.h"  // complete type: TSubclassOf<UZZZBasicAttack> conversions need StaticClass()
#include "ZZZCharacter.h"

UZZZFollowUpAttack::UZZZFollowUpAttack()
{
}

void UZZZFollowUpAttack::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Decision-window end (2026-08-16): the perfect-dodge player slow
	// (GE_PlayerSlowMotion, grants State.SlowMotion via its TargetTags
	// component) exists only to give the player time to input the counter.
	// Once the counter activates, the slow ends — the attack plays at normal
	// speed. Removal is synchronous (attribute recompute + bridge mirror
	// happen inside this call), so CustomTimeDilation is back to 1.0 before
	// the montage starts. A no-op on the dash-attack path (no slow exists
	// after a plain dodge).
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		FGameplayTagContainer SlowTags;
		SlowTags.AddTag(FZZZGameplayTags::Get().State_SlowMotion);
		ASC->RemoveActiveEffectsWithGrantedTags(SlowTags);
	}

	// 支援突击穿敌 warp (2026-09-06): 激活时角色持有招架目标敌人 → 取走并清空引用
	// (防后续连段中的 DashAttack 等 FollowUpAttack 误触发), 落点 = 敌人正后方,
	// 旋转 = 位移方向(穿敌后保持冲刺朝向)。目标在蒙太奇开始前写入; 蒙太奇上的
	// MotionWarping 窗口(须与 RushWarpTargetName 一致)逐帧扭曲 root motion, 使其
	// 跨过敌人落到身后; CollisionPassThrough notify 负责穿敌碰撞(并授 State.PassThrough
	// 停 RotateToTarget 转向)。非招架链(普攻/闪避追击)无招架敌人 → 本段跳过, 行为不变。
	if (AZZZCharacter* Owner = Cast<AZZZCharacter>(GetAvatarActorFromActorInfo()))
	{
		if (AZZZCombatEnemy* ParryEnemy = Owner->GetParryEnemy())
		{
			Owner->ClearParryEnemy();
			if (UMotionWarpingComponent* MotionWarping = Owner->GetMotionWarping())
			{
				const FVector EnemyLocation = ParryEnemy->GetActorLocation();
				const FVector EnemyForward = ParryEnemy->GetActorForwardVector();
				const FVector OwnerLocation = Owner->GetActorLocation();
				const FVector WarpLocation = EnemyLocation - EnemyForward * RushPassDistance;
				FRotator DashRotation = (WarpLocation - OwnerLocation).Rotation();
				DashRotation.Pitch = 0.0f;
				DashRotation.Roll = 0.0f;
				MotionWarping->AddOrUpdateWarpTargetFromLocationAndRotation(
					RushWarpTargetName, WarpLocation, DashRotation);
			}
		}
	}

	// Single-strike template: rotate toward the target, then play the montage.
	// Damage is delivered by ZZZAnimNotify_AttackTrace on the montage; the
	// base class ends the ability on montage completion / blend-out.
	if (bRotateToTarget)
	{
		RotateToTargetTask = UAbilityTask_RotateToTarget::RotateToTarget(
			this, TargetSearchRadius, RotateInterpSpeed);
		RotateToTargetTask->ReadyForActivation();
	}

	if (!PlayAttackMontage())
	{
		return;  // PlayAttackMontage already ended the ability (null montage)
	}

	// Optional combo handoff (2026-08-29): when NextComboAbility is configured
	// (e.g. GA_DashAttack → GA_BasicAttack_02), an attack input inside the
	// montage's recovery CanCombo window chains straight into the next hit
	// (base class TrySetupComboHandoff). Unset = terminal single strike,
	// exactly as before — no combo tasks are spawned.
	if (NextComboAbility)
	{
		TrySetupComboHandoff();
	}
}
