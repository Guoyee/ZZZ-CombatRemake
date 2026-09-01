// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZEnemyAttack.h"
#include "AbilitySystemComponent.h"
#include "AbilityTask_RotateToTarget.h"
#include "Tags/ZZZGameplayTags.h"

UZZZEnemyAttack::UZZZEnemyAttack()
{
	// Ability tags / activation owned tags are configured on the Blueprint
	// (GA_EnemyAttack) — see header comment for the CDO timing rationale.
}

void UZZZEnemyAttack::ActivateAbility(
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

	// Optional auto-aim toward the player (mirrors UZZZBasicAttack).
	if (bRotateToTarget)
	{
		RotateToTargetTask = UAbilityTask_RotateToTarget::RotateToTarget(
			this, TargetSearchRadius, RotateInterpSpeed);
		RotateToTargetTask->ReadyForActivation();
	}

	// Montage playback + EndAbility wiring lives in the base class (Task 1a).
	if (!PlayAttackMontage())
	{
		return;  // PlayAttackMontage already ended the ability (null montage)
	}
}

void UZZZEnemyAttack::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// Window-tag fallback (CLAUDE.md rule 1 layer A): the wind-up window
	// (Effect.Enemy.AttackWindow, granted by an AbilityWindow notify state on
	// AM_EnemyAttack) must not survive the ability — e.g. the attack was
	// cancelled by a hit (CancelAbilities) before NotifyEnd ran. A stale window
	// would make the player's NEXT dodge judge as perfect (FindNearestEnemy
	// queries this tag). Removal of a not-granted tag is a no-op — safe.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->RemoveLooseGameplayTag(FZZZGameplayTags::Get().Effect_Enemy_AttackWindow);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
