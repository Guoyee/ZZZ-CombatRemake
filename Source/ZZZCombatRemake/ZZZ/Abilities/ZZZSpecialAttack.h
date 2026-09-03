// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ZZZGameplayAbility.h"
#include "ZZZSpecialAttack.generated.h"

class UAbilityTask_RotateToTarget;

/**
 * Special attack (特殊技) — Y key, no cooldown, single GA branching on
 * ENERGY and on ENTRY CONTEXT. One Blueprint subclass per character; the
 * normal / enhanced / full / quick matrix is all data:
 *
 *   GA_SpecialAttack_*  — Asset Tags = { Ability.Attack.Basic,
 *                                        Ability.Attack.Special }
 *                          (Basic: the basic-attack starter suppresses itself
 *                           while this plays, the switch-out wait defaults to
 *                           Ability.Attack.Basic, and the dodge's
 *                           CancelAbilities(Basic) can cancel it — all intended.
 *                           Special: spec location + self-chain guard.)
 *                          AttackMontage    = 普通特殊技蒙太奇
 *                          EnhancedMontage  = 强化特殊技蒙太奇
 *                          EnergyCost       = 强化门槛 == 消耗 (默认 50)
 *                          bRotateToTarget  = true
 *
 *   Runtime branch (energy lives HERE, never in the character gate):
 *     Energy >= EnergyCost  → EnhancedMontage, spend EnergyCost (SetByCaller
 *                             Data.Energy GE — engine AbilityCosts can't carry
 *                             per-instance SetByCaller magnitudes).
 *     Energy <  EnergyCost  → AttackMontage, no cost.
 *
 *   Entry-context branch (quick strike, 2026-09-03): a special pressed inside
 *   a basic-attack quick-entry window (段 2/4 — character gate decides) skips
 *   strike 1 and starts at the strike-2 section. The flag rides the
 *   TriggerEventData of the character's TryActivateAbility call
 *   (Event.Combat.SpecialQuickEntry) — NO AbilityTriggers on this GA (a
 *   gameplay-event trigger would double-fire with the gate) and NO notify on
 *   the basic montages. The special montages carry a section named
 *   "QuickStrike" whose start frame is the strike-2 entry pose; full entries
 *   play the whole montage linearly (both strikes, two damage notifies).
 *
 *   Montage lifecycle: the whole montage belongs to the GA (structure A — no
 *   EndEventTag notify, no window notifies), base-class montage callbacks end
 *   the ability on completion/interruption. 收刀之后是自由态：攻击从普攻第 1 段
 *   重新起手。Do NOT copy BasicAttack notify layouts into these montages.
 *
 *   Cleanup note: a perfect-dodge player slow (State.SlowMotion GE) is removed
 *   on activation (FollowUp template) — otherwise the special plays at 0.5x.
 */
UCLASS(Abstract)
class UZZZSpecialAttack : public UZZZGameplayAbility
{
	GENERATED_BODY()

public:
	UZZZSpecialAttack();

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
	// === Variant montages ===

	// 普通特殊技 = 基类 AttackMontage 属性（含打击 1 + 打击 2 全流程 + QuickStrike section）。

	/** 强化特殊技蒙太奇（能量 ≥ EnergyCost 时播放——同构：两击全流程 + QuickStrike section）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Special")
	TObjectPtr<UAnimMontage> EnhancedMontage;

	/** 强化门槛 == 消耗：Energy >= EnergyCost 触发强化版并扣除 EnergyCost。默认 50（Max 100）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Special",
		meta = (ClampMin = "0.0"))
	float EnergyCost = 50.0f;

	/** 蒙太奇内"打击 2 衔接"section 名——快速派生从该 section 起播。 */
	static const FName QuickEntrySectionName;

	// === Targeting (same trio as UZZZFollowUpAttack) ===

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
