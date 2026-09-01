// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "ZZZGameplayAbility.generated.h"

class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;
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

	/** The next combo ability to activate on successful transition. nullptr = terminal hit. */
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
	 * Configure on the Blueprint:
	 *   GA_Dodge:         Event.Combat.DodgeEnd
	 *   GA_BasicAttack_N: Event.Combat.AttackEnd
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Animation")
	FGameplayTag EndEventTag;

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

	/** Same as PlayAttackMontage but plays the given montage (e.g. direction-picked dodge). */
	bool PlayMontage(UAnimMontage* Montage);

	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageBlendOut();

	UFUNCTION()
	void OnMontageInterrupted();

	UFUNCTION()
	void OnMontageCancelled();

	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;
};
