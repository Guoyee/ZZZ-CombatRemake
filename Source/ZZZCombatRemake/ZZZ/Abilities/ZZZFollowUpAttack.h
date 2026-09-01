// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ZZZGameplayAbility.h"
#include "ZZZFollowUpAttack.generated.h"

class UAbilityTask_RotateToTarget;

/**
 * Dodge-window follow-up attacks — the single-strike attacks fired by the
 * attack input while a dodge's displacement window (Effect.Ability.CanDashAttack)
 * is open. Two Blueprint subclasses exist, distinguished ONLY by data:
 *
 *   GA_DashAttack   — dash attack  (冲刺攻击):  AM_Attack_Rush
 *                     AbilityTriggers=[Input.Attack]
 *                     ActivationRequiredTags=[Effect.Ability.CanDashAttack]
 *                     ActivationBlockedTags=[State.PerfectDodge]
 *   GA_DashCounter  — dodge counter (闪避反击):  AM_Attack_Counter
 *                     AbilityTriggers=[Input.Attack]
 *                     ActivationRequiredTags=[Effect.Ability.CanDashAttack, State.PerfectDodge]
 *
 * The input routing lives in AZZZCharacter::Input_AbilityInputTagPressed: the
 * ASC HandleGameplayEvent(Input.Attack) trigger (AbilityTriggers, TriggerSource
 * defaults to GameplayEvent) fires this family; the character gate skips the
 * basic-attack starter while a dodge is active inside the CanDashAttack window.
 *
 * Execution template (shared with neither combo nor dodge):
 *   CommitAbility → optional RotateToTarget → PlayAttackMontage → end on
 *   montage completion (base class callbacks). No combo chaining, no input
 *   buffering — a follow-up attack is a terminal single strike; damage comes
 *   from ZZZAnimNotify_AttackTrace on the montage.
 *
 * The CanDashAttack window is granted by an AnimNotifyState_AbilityWindow on
 * the dodge montage and removed by UZZZDodge::EndAbility as a fallback — no
 * tag cleanup needed here.
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
