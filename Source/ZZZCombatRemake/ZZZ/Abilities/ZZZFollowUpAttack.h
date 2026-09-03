// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ZZZGameplayAbility.h"
#include "ZZZFollowUpAttack.generated.h"

class UAbilityTask_RotateToTarget;

/**
 * Dodge-window follow-up attacks — the single-strike attacks fired by the
 * attack input while a dodge's displacement window is open. 2026-09-03: the
 * window tag was unified — dodge montages now grant the generic
 * Effect.Ability.CanCombo (AbilityWindow notify, layer A) instead of the
 * retired Effect.Ability.CanDashAttack. Two Blueprint subclasses exist,
 * distinguished ONLY by data:
 *
 *   GA_DashAttack   — dash attack  (冲刺攻击):  AM_Attack_Rush
 *                     AbilityTriggers=[Event.Combat.AttackFollowUp]
 *                     ActivationRequiredTags=[Effect.Ability.CanCombo]
 *                     ActivationBlockedTags=[State.PerfectDodge]
 *   GA_DashCounter  — dodge counter (闪避反击):  AM_Attack_Counter
 *                     AbilityTriggers=[Event.Combat.AttackFollowUp]
 *                     ActivationRequiredTags=[Effect.Ability.CanCombo, State.PerfectDodge]
 *
 * The input routing lives in AZZZCharacter::Input_AbilityInputTagPressed: the
 * character gate, while a dodge is active inside its CanCombo window, converts
 * the attack press into HandleGameplayEvent(Event.Combat.AttackFollowUp) —
 * a DEDICATED event so a plain Input.Attack broadcast can never fire this
 * family from a basic-combo window (which also carries CanCombo). The gate
 * also skips the basic-attack starter in that branch.
 *
 * Execution template (shared with neither combo nor dodge):
 *   CommitAbility → optional RotateToTarget → PlayAttackMontage → end on
 *   montage completion (base class callbacks). No combo chaining, no input
 *   buffering — a follow-up attack is a terminal single strike; damage comes
 *   from ZZZAnimNotify_AttackTrace on the montage.
 *
 * The window is granted by an AnimNotifyState_AbilityWindow on the dodge
 * montage and removed by UZZZDodge::EndAbility as a fallback (CanCombo) — no
 * tag cleanup needed here. A follow-up's own tail may carry its own CanCombo
 * window (chaining into basic 02 / admitting a special attack).
 */
UCLASS(Abstract)
class UZZZFollowUpAttack : public UZZZGameplayAbility
{
	GENERATED_BODY()

public:
	UZZZFollowUpAttack();

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
