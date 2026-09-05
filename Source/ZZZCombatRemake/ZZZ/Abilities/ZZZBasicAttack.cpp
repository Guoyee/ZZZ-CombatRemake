// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZBasicAttack.h"
#include "AbilityTask_RotateToTarget.h"

UZZZBasicAttack::UZZZBasicAttack()
{
}

void UZZZBasicAttack::ActivateAbility(
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

	// Montage playback (template lifted to the base class — Task 1a)
	if (!PlayAttackMontage())
	{
		return;  // PlayAttackMontage already ended the ability (null montage)
	}

	// Task 1: RotateToTarget (optional — auto-aim toward nearest enemy)
	if (bRotateToTarget)
	{
		RotateToTargetTask = UAbilityTask_RotateToTarget::RotateToTarget(
			this, TargetSearchRadius, RotateInterpSpeed);
		RotateToTargetTask->ReadyForActivation();
	}

	// Task 2+3: 双窗口输入支持 (input buffer + combo) — 基类 TrySetupComboHandoff
	// (2026-09-05 下沉) 统一武装 WaitInputBuffer(CanBuffer 死区预按) + WaitCombo
	// (CanCombo 窗消费/交接)。Spawned unconditionally: 非终段驱动连段; 终段其
	// 关窗分支 = 陈旧缓冲 flush。窗口 tag 兜底清理由基类 EndAbility 统一负责。
	TrySetupComboHandoff();
}
