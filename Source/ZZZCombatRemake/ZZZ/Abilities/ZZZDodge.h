// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"  // FActiveGameplayEffectHandle (player slow GE handle)
#include "ZZZGameplayAbility.h"
#include "ZZZDodge.generated.h"

class UAbilityTask_ApplyRootMotionConstantForce;
class UGameplayEffect;
class UZZZFollowUpAttack;

/**
 * Dodge ability — i-frames + displacement.
 *
 * Configure on the Blueprint (GA_Dodge):
 *   - Activation Owned Tags = State.Invulnerable  (engine grants for the whole
 *     ability lifetime and removes on ANY end path — cancel/interrupt included)
 *   - Ability Tags           = Ability.Defense.Dodge (identity for the input gate)
 *   - AttackMontage          = AM_Dodge
 *
 * Perfect dodge (2026-08-09 design change — judgment moved to press time):
 * ActivateAbility queries the nearest enemy within PerfectDodgeDetectRadius
 * carrying Effect.Enemy.AttackWindow (an AnimNotifyState_AbilityWindow on the
 * ENEMY's attack montage, synced with the yellow-flash wind-up warning). If
 * found, the dodge is "perfect" — pressing inside the flash means the
 * incoming attack is successfully evaded, no hit-frame confirmation needed.
 * The enemy is flagged (Effect.Enemy.Dodged); the slow-mo is paid later by a
 * consume notify (UZZZAnimNotify_EnemyDodgeSlow) on the ENEMY's attack
 * montage placed after the damage frame — the slow starts once the strike
 * has been thrown and missed (打空后), not at the dodge press (2026-09-03).
 * A lighter slow on ourselves (PlayerSlowMotionEffect, 0.5) stays
 * animator-driven (PlayerSlowEventTag notify on the displacement tail).
 * No camera shake — the slow motion itself is the reward (2026-08-16).
 * A dodge pressed outside any window is a plain dodge: i-frames + displacement,
 * no reward. The i-frame damage absorb itself (State.Invulnerable intercept in
 * the AttributeSet) is unchanged.
 *
 * Displacement:
 *   - Root-motion montage (bUseProceduralDisplacement=false): the animation
 *     carries the movement.
 *   - Placeholder montage (bUseProceduralDisplacement=true, default): an
 *     ApplyRootMotionConstantForce task drives the movement — direction from
 *     the last movement input, fallback to character backward.
 */
UCLASS(Abstract)
class UZZZDodge : public UZZZGameplayAbility
{
	GENERATED_BODY()

public:
	UZZZDodge();

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
	// === Perfect-dodge detection (2026-08-09: judgment at press time) ===

	/** Perfect-dodge detection radius (cm) — nearest enemy in an attack window. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Dodge|PerfectDodge")
	float PerfectDodgeDetectRadius = 500.0f;

	/**
	 * Effect applied to OURSELVES on a perfect dodge (GE_PlayerSlowMotion,
	 * TimeDilation 0.5) — the lighter decision-window slow. Applied via
	 * ApplyGameplayEffectSpecToSelf on our own ASC; the TimeDilation attribute
	 * bridge on AZZZCharacter mirrors it to CustomTimeDilation.
	 *
	 * NOT applied at activation — the displacement section plays at normal
	 * speed (dodging should FEEL fast). Applied when the animator-placed
	 * AnimNotify_SendGameplayEvent for PlayerSlowEventTag fires (on the
	 * montage's displacement tail), so the timing is fully art-controlled.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Dodge|PerfectDodge")
	TSubclassOf<UGameplayEffect> PlayerSlowMotionEffect;

	/**
	 * Status effect carrying State.PerfectDodge (GE_PerfectDodge_Status:
	 * Duration 0.5s FIXED + TargetTags=State.PerfectDodge — no SetByCaller).
	 * Applied to ourselves when the dodge starts inside an enemy attack
	 * window; routes the follow-up attack input to GA_DodgeCounter
	 * (Required=State.PerfectDodge) instead of GA_DashAttack
	 * (Blocked=State.PerfectDodge). The fixed duration counts WORLD time — a
	 * duration GE expires via a timer on the world's FTimerManager
	 * (FActiveGameplayEffect::DurationHandle → CheckDurationExpired), so the
	 * actor's CustomTimeDilation does NOT stretch it (2026-08-15 verification).
	 * The counter window is a fixed 0.5s real. Layer E (local routing flag):
	 * Duration GE self-expires, no timer / handle management. Not granted via
	 * LooseTag — dash-counter routing is gameplay logic.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Dodge|PerfectDodge")
	TSubclassOf<UGameplayEffect> PerfectDodgeEffect;

	/**
	 * Event tag fired by an AnimNotify_SendGameplayEvent on the dodge montage's
	 * displacement tail — the moment the player slow begins. Animator-placed,
	 * independent from EndEventTag (DodgeEnd). Configure on GA_Dodge:
	 * Event.Combat.DodgeSlowStart. NOTE: the notify must live inside the
	 * ability lifetime (before the DodgeEnd notify that ends the ability) —
	 * the listener is removed on EndAbility.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Dodge|PerfectDodge")
	FGameplayTag PlayerSlowEventTag;

	/** Fires on PlayerSlowEventTag — applies the player slow (perfect dodge only). */
	void OnPlayerSlowStart();

	// === Follow-up strike handoff (2026-09-03 — 追击技 = 闪避的连段段) ===

	/**
	 * The dodge's displacement tail carries the generic CanCombo window (same
	 * AbilityWindow notify as basic-combo segments — CanDashAttack retired).
	 * An attack input inside that window hands off to the follow-up strike
	 * through the base-class combo machinery (TrySetupComboHandoff +
	 * WaitCombo) instead of engine AbilityTriggers — dash attack / dodge
	 * counter are combo segments, configured per character as ability class
	 * refs here (GA_Dodge BP):
	 *   DashFollowUpAbility   — plain dodge   → GA_DashAttack
	 *   PerfectFollowUpAbility— perfect dodge → GA_DashCounter
	 * Branch decided at press time (bIsPerfectDodge, 判定前移 philosophy) via
	 * GetComboNext(); State.PerfectDodge keeps its tag/GE for other systems.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Dodge|FollowUp")
	TSubclassOf<UZZZFollowUpAttack> DashFollowUpAbility;

	/** 完美闪避的追击段（闪避反击 GA_DashCounter）。空 = 该情形无追击、窗口内攻击走普攻起手。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Dodge|FollowUp")
	TSubclassOf<UZZZFollowUpAttack> PerfectFollowUpAbility;

	/** 组合交接取下一段：完美闪避 → 反击，普通 → 冲刺攻击。 */
	virtual TSubclassOf<UGameplayAbility> GetComboNext() const override;

	// === Animation (direction-picked — one ability, two montages) ===

	/** Forward-dodge montage, played when there is movement input. Carries its own root motion. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Dodge")
	TObjectPtr<UAnimMontage> ForwardMontage;

	/** Backward-dodge montage, played on neutral input. Carries its own root motion. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Dodge")
	TObjectPtr<UAnimMontage> BackMontage;

	// === Displacement (fallback only — real montages carry root motion) ===

	/** True: drive movement with an ApplyRootMotionConstantForce task instead of montage root motion. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Dodge")
	bool bUseProceduralDisplacement = false;

	/** Constant force acceleration (cm/s²) — ~11000 × 0.22s ≈ 260cm of dodge. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Dodge",
		meta = (EditCondition = "bUseProceduralDisplacement"))
	float DodgeAcceleration = 11000.0f;

	/** Duration (s) of the procedural dodge force. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Dodge",
		meta = (EditCondition = "bUseProceduralDisplacement"))
	float DodgeDuration = 0.22f;

	// === Double-dodge cooldown ===

	/**
	 * Window (s) after a dodge during which a second dodge is allowed but arms
	 * the cooldown — a dodge while cooling down is rejected.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Dodge")
	float DoubleDodgeWindow = 0.7f;

	/** Cooldown (s) armed by a double-dodge inside DoubleDodgeWindow. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Dodge")
	float DodgeCooldown = 0.7f;

	UPROPERTY()
	TObjectPtr<UAbilityTask_ApplyRootMotionConstantForce> RootMotionTask;

private:
	float LastDodgeTime = -FLT_MAX;
	float DodgeCooldownUntil = -FLT_MAX;

	/** True for this activation while a perfect-dodge window was detected — gates the player slow. */
	bool bIsPerfectDodge = false;

	/** Handle of the active player slow GE — removed on any new dodge so the displacement always plays at normal speed. */
	FActiveGameplayEffectHandle PlayerSlowEffectHandle;

	/** Listener handle on GenericGameplayEventCallbacks for PlayerSlowEventTag (removed in EndAbility). */
	FDelegateHandle PlayerSlowEventHandle;
};
