// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "GameplayTagContainer.h"
#include "AbilityTask_WaitCombo.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWaitComboDelegate);

/**
 * Event-driven combo trigger. No Tick.
 *
 * CanCombo opens → check PlayerController buffer.
 *   Buffered → combo immediately.
 *   Not buffered → register Attack listener.
 * CanCombo closes → unregister Attack.
 *
 * Move interruption is handled in Character::Move() via CancelAbilitiesWithTag.
 */
UCLASS()
class UAbilityTask_WaitCombo : public UAbilityTask
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FWaitComboDelegate OnComboTriggered;

	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks",
		meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_WaitCombo* WaitCombo(
		UGameplayAbility* OwningAbility,
		FGameplayTag InComboWindowTag,
		FGameplayTag InAttackInputTag);

	virtual void Activate() override;
	virtual void OnDestroy(bool bInOwnerFinished) override;

protected:
	void OnComboWindowChanged(FGameplayTag Tag, int32 NewCount);
	void OnAttackInput(const FGameplayEventData* Payload);

	FGameplayTag ComboWindowTag;
	FGameplayTag AttackInputTag;

	bool bHasTriggered = false;

	FDelegateHandle TagChangeHandle;
	FDelegateHandle AttackHandle;
};
