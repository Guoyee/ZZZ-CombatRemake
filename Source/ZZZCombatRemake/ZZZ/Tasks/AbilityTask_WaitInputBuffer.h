// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "GameplayTagContainer.h"
#include "AbilityTask_WaitInputBuffer.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWaitInputBufferDelegate);

/**
 * Listens for InputTag on the ASC and writes to PlayerController's buffer
 * while Effect.Input.CanBuffer is active (managed by AnimNotifyState_InputWindow).
 */
UCLASS()
class UAbilityTask_WaitInputBuffer : public UAbilityTask
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FWaitInputBufferDelegate OnInputBuffered;

	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks",
		meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_WaitInputBuffer* WaitInputBuffer(
		UGameplayAbility* OwningAbility,
		FGameplayTag InInputTag);

	virtual void Activate() override;
	virtual void OnDestroy(bool bInOwnerFinished) override;

protected:
	void OnInputPressed(const FGameplayEventData* Payload);

	FGameplayTag InputTag;
	FDelegateHandle InputPressedHandle;
};
