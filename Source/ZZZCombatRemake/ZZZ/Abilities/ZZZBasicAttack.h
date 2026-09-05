// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ZZZGameplayAbility.h"
#include "ZZZBasicAttack.generated.h"

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
 *   2. Two-window input support — the base class TrySetupComboHandoff()
 *      (2026-09-05 下沉基类) arms BOTH WaitInputBuffer (CanBuffer 死区预按 →
 *      PC 缓冲) and WaitCombo (CanCombo 窗消费缓冲/实时按键 → 交接下一段):
 *      a combo-window input activates NextComboAbility first, then ends this
 *      one (no blend gap). Spawned unconditionally — on terminal hits the
 *      window-close branch is the combo system's stale-buffer flush.
 *   Window-tag fallback (CanCombo/CanBuffer) 由基类 EndAbility 统一兜底。
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

protected:
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
