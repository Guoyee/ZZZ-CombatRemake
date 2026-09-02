// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ZZZGameplayAbility.h"
#include "ZZZBasicAttack.generated.h"

class UAbilityTask_WaitInputBuffer;
class UAbilityTask_RotateToTarget;

/**
 * Core combo attack ability — orchestrates montage playback, input buffering,
 * and combo window checking for a single hit in the basic attack chain.
 *
 * Each GA_BasicAttack_N Blueprint configures:
 *   - ComboIndex:         which hit this is (1-4)
 *   - NextComboAbility:   the follow-up ability class
 *   - AttackMontage:      the montage to play
 *
 * On activation, this ability spawns:
 *   1. PlayMontageAndWait — plays the attack montage (base class template)
 *   2. WaitInputBuffer    — captures buffered input after dead zone
 *   3. Combo handoff      — WaitCombo + transition via the base class opt-in
 *      TrySetupComboHandoff() (2026-08-29): a combo-window input activates
 *      NextComboAbility first, then ends this one (no blend gap). Spawned
 *      unconditionally — on terminal hits its window-close branch is the
 *      combo system's stale-buffer flush.
 *
 * This is the central orchestrator of the combo system.
 * All combo timing is driven by montage notifies, not hardcoded floats.
 */
UCLASS(Abstract)
class UZZZBasicAttack : public UZZZGameplayAbility
{
	GENERATED_BODY()

public:
	UZZZBasicAttack();

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
	// === Managed tasks ===

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitInputBuffer> InputBufferTask;

	// === Targeting ===

	/** When enabled, the character auto-rotates toward the nearest enemy during this attack. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Targeting")
	bool bRotateToTarget = true;

	/** Sphere radius (cm) for finding the nearest enemy at ability activation. */
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
