// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ZZZGameplayAbility.h"
#include "ZZZBasicAttack.generated.h"

class UAbilityTask_WaitInputBuffer;
class UAbilityTask_WaitCombo;
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
 * On activation, this ability spawns three AbilityTasks:
 *   1. PlayMontageAndWait    — plays the attack montage
 *   2. WaitInputBuffer       — captures buffered input after dead zone
 *   3. WaitCombo             — checks CanCombo + Buffered each tick
 *
 * When WaitCombo fires → CheckComboTransition() → activates next ability,
 * then ends this one (in that order, to avoid blend gaps).
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
	// === Combo callbacks ===

	UFUNCTION()
	void OnComboTriggered();

	void CheckComboTransition();

	// === Managed tasks ===

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitInputBuffer> InputBufferTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitCombo> ComboCheckTask;

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
