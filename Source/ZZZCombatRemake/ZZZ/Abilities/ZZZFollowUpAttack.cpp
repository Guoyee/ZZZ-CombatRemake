// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZFollowUpAttack.h"
#include "AbilitySystemComponent.h"
#include "AbilityTask_RotateToTarget.h"
#include "Tags/ZZZGameplayTags.h"
#include "ZZZBasicAttack.h"  // complete type: TSubclassOf<UZZZBasicAttack> conversions need StaticClass()

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
