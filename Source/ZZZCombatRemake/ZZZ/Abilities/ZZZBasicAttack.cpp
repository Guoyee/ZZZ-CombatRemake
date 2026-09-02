// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZBasicAttack.h"
#include "AbilitySystemComponent.h"
#include "ZZZGameplayTags.h"
#include "AbilityTask_WaitInputBuffer.h"
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

	// Task 2: WaitInputBuffer
	InputBufferTask = UAbilityTask_WaitInputBuffer::WaitInputBuffer(
		this, FZZZGameplayTags::Get().Input_Attack);
	InputBufferTask->ReadyForActivation();

	// Task 3: combo handoff — WaitCombo + transition moved to the base class
	// opt-in helper (2026-08-29). Spawned unconditionally: on non-terminal
	// hits it drives the chain; on terminal hits its window-close branch is
	// the stale-buffer flush.
	TrySetupComboHandoff();
}

void UZZZBasicAttack::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// Window-tag fallback (CLAUDE.md rule 1 layer A): a montage interruption
	// may skip AnimNotifyState::NotifyEnd, which would leave CanCombo /
	// CanBuffer stuck on the ASC and wrongly route the next attack's input.
	// Removal of a not-granted tag is a no-op — safe unconditionally.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		const FZZZGameplayTags& GameplayTags = FZZZGameplayTags::Get();
		ASC->RemoveLooseGameplayTag(GameplayTags.Effect_Ability_CanCombo);
		ASC->RemoveLooseGameplayTag(GameplayTags.Effect_Input_CanBuffer);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
