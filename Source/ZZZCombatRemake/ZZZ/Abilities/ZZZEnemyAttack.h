// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ZZZGameplayAbility.h"
#include "ZZZEnemyAttack.generated.h"

class UAbilityTask_RotateToTarget;

/**
 * Minimal enemy attack ability.
 *
 * Deliberately NOT a UZZZBasicAttack: that class runs player-only tasks
 * (WaitInputBuffer / WaitCombo) which an enemy would waste. This one is just
 * Commit → optional RotateToTarget → PlayAttackMontage (template lifted to
 * UZZZGameplayAbility in Task 1a).
 *
 * Configure on the Blueprint (GA_EnemyAttack):
 *   - Ability Tags          = Ability.Attack.Enemy   (CancelAbilities target for parry)
 *   - Activation Owned Tags = State.Attacking         (switch auto-judgment; engine
 *                                                      grants/removes with the ability,
 *                                                      covers cancel/interrupt paths)
 *   - AttackMontage          = AM_EnemyAttack
 *
 * Not configured in the native ctor on purpose: native ability CDOs are built
 * during module load, before FZZZGameplayTags is populated (see CLAUDE.md
 * 实施经验 #2).
 *
 * This C++ class doubles as the Phase 6 StateTree attack hook.
 */
UCLASS(Abstract)
class UZZZEnemyAttack : public UZZZGameplayAbility
{
	GENERATED_BODY()

public:
	UZZZEnemyAttack();

	// === UGameplayAbility overrides ===

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	// === Targeting ===

	/** When enabled, the enemy auto-rotates toward the player during this attack. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Targeting")
	bool bRotateToTarget = true;

	/** Sphere radius (cm) for finding the player at ability activation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Targeting",
		meta = (EditCondition = "bRotateToTarget"))
	float TargetSearchRadius = 500.0f;

	/** Yaw interpolation speed toward target (higher = snappier, 5-15 typical). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Targeting",
		meta = (EditCondition = "bRotateToTarget"))
	float RotateInterpSpeed = 10.0f;

	UPROPERTY()
	TObjectPtr<UAbilityTask_RotateToTarget> RotateToTargetTask;
};
