// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "ZZZGameplayAbility.generated.h"

class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitCombo;
class UZZZBasicAttack;
class AZZZCombatEnemy;

/**
 * Base class for all ZZZ combat abilities.
 *
 * Provides common properties shared across attack, dodge, assist, and ultimate abilities:
 * - ComboIndex: which hit in the combo sequence this is
 * - NextComboAbility: the follow-up ability class (null for terminal hits)
 * - AttackMontage: the montage to play on activation
 *
 * Subclass this in Blueprint for each specific ability (e.g. GA_BasicAttack_01).
 */
UCLASS(Abstract)
class UZZZGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UZZZGameplayAbility();

	// === Convenience accessors ===

	/** Gets the ASC from the current actor info. Returns nullptr if not available. */
	UFUNCTION(BlueprintCallable, Category = "ZZZ|Ability")
	UAbilitySystemComponent* GetZZZASC() const;

	/**
	 * Nearest living, visible enemy within Radius that carries RequiredState
	 * (empty tag = any enemy). Filters hidden actors and State.Dead. Used by
	 * the dodge's perfect-dodge window query and (Phase 3 Task 5b) the switch
	 * auto-judgment.
	 */
	AZZZCombatEnemy* FindNearestEnemy(
		float Radius, const FGameplayTag& RequiredState = FGameplayTag()) const;

	/** Gets the avatar actor (the character). */
	UFUNCTION(BlueprintCallable, Category = "ZZZ|Ability")
	AActor* GetZZZAvatarActor() const;

	// === Combo configuration (set per Blueprint subclass) ===

	/** Which hit in the combo this ability represents (1 = first hit, 2 = second, etc.). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Combo")
	int32 ComboIndex = 1;

	/**
	 * The next combo ability to activate on a combo-window input (opt-in chain
	 * handoff — consumed by TrySetupComboHandoff, which subclasses call when
	 * they want combo chaining; nullptr = terminal hit). Combo transitions
	 * call TryActivateAbilityByClass(Next) FIRST, then EndAbility(this) — the
	 * new montage's BlendIn overlaps this ability's recovery (收刀), avoiding
	 * a gap frame. Type is UZZZBasicAttack so the chain always lands on a
	 * basic-attack hit (e.g. GA_DashAttack → GA_BasicAttack_02).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Combo")
	TSubclassOf<UZZZBasicAttack> NextComboAbility;

	// === Animation ===

	/** The attack montage to play when this ability activates. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Animation")
	TObjectPtr<UAnimMontage> AttackMontage;

	/**
	 * Optional event that ends this ability early while the montage keeps
	 * playing (PlayMontage uses bStopWhenAbilityEnds=false). Used for
	 * two-section montages — the front section is the action (dodge motion /
	 * attack hit), the back section a transition (stand-up / 收刀) that plays
	 * unowned while the character is fully actionable again.
	 * Default when unset (2026-08-29): Event.Combat.AttackEnd — a two-section
	 * montage ends its GA at the action-section notify unless a Blueprint
	 * overrides (GA_Dodge: Event.Combat.DodgeEnd). Resolved lazily via
	 * GetEndEventTag() — the CDO is built before native tags register
	 * (CLAUDE.md 规则 2), so the ctor's RequestGameplayTag may be invalid.
	 * Configure on the Blueprint:
	 *   GA_Dodge:         Event.Combat.DodgeEnd
	 *   GA_BasicAttack_N: Event.Combat.AttackEnd
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Animation")
	FGameplayTag EndEventTag;

	/** EndEventTag, falling back to Event.Combat.AttackEnd when unset. */
	FGameplayTag GetEndEventTag() const;

	/** Handle for the EndEventTag listener (GenericGameplayEventCallbacks). */
	FDelegateHandle EndEventHandle;

	// === UGameplayAbility overrides (early-end listener management) ===

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
	// === Montage playback (shared template for all montage-driven abilities) ===

	/**
	 * Plays AttackMontage via PlayMontageAndWait and wires the four montage
	 * callbacks to EndAbility. Returns false and ends the ability (cancelled)
	 * when no montage is configured — callers bail out on false.
	 */
	bool PlayAttackMontage();

	/**
	 * Same as PlayAttackMontage but plays the given montage (e.g. direction-picked
	 * dodge). StartSection (optional, 2026-09-03) is forwarded to
	 * PlayMontageAndWait — section-branch entries (UZZZSpecialAttack's quick
	 * strike) start mid-montage; NAME_None = play from the first section.
	 */
	bool PlayMontage(UAnimMontage* Montage, FName StartSection = NAME_None);

	// virtual (2026-09-03): UZZZSpecialAttack overrides OnMontageCompleted to
	// stage lead-in → body playback instead of ending on the first montage.
	UFUNCTION()
	virtual void OnMontageCompleted();

	UFUNCTION()
	virtual void OnMontageBlendOut();

	UFUNCTION()
	virtual void OnMontageInterrupted();

	UFUNCTION()
	virtual void OnMontageCancelled();

	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	// === Optional combo handoff (opt-in chain support) ===

	/**
	 * Spawns a WaitCombo task on the CanCombo window (2026-08-29, moved up from
	 * UZZZBasicAttack so any attack-family subclass can opt in with one call).
	 * A combo-window attack input triggers OnComboHandoffTriggered — the next
	 * ability activates, then this one ends (transition order matters: Next
	 * first, BlendIn overlaps the recovery, no gap frame). Spawned even on
	 * terminal hits (NextComboAbility null) — the task's window-close branch
	 * is the combo system's stale-buffer flush (WaitCombo.cpp), so callers
	 * with recovery windows should call this unconditionally (BasicAttack
	 * does); callers without windows may gate on NextComboAbility.
	 */
	void TrySetupComboHandoff();

	UFUNCTION()
	void OnComboHandoffTriggered();

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitCombo> ComboHandoffTask;
};
