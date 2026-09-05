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

		// Same double-track fallback for the perfect-dodge flag (2026-09-03):
		// Effect.Enemy.Dodged is granted by UZZZDodge's press-time judgment and
		// consumed by the UZZZAnimNotify_EnemyDodgeSlow on this montage. If the
		// attack ends before that notify (cancelled / interrupted / montage
		// never reached it), the stale flag must not linger — a LATER attack's
		// notify would wrongly consume it and pay a slow for a dodge that
		// never happened against it.
		ASC->RemoveLooseGameplayTag(FZZZGameplayTags::Get().Effect_Enemy_Dodged);

		// Same double-track fallback for the parry flag (2026-09-04):
		// Effect.Enemy.ParryPending is granted by the PC's parry judgment and
		// consumed by the UZZZAnimNotify_EnemyParryImpact on this montage. If
		// the attack ends before that notify (hit-cancel / death / montage
		// never reached it), the stale flag must not linger — a LATER attack's
		// notify would wrongly freeze the enemy for a parry that never happened.
		ASC->RemoveLooseGameplayTag(FZZZGameplayTags::Get().Effect_Enemy_ParryPending);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
