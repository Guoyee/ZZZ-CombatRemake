// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "AnimNotify_SendGameplayEvent.generated.h"

/**
 * Generic AnimNotify that broadcasts a GameplayEvent to the owner's ASC.
 *
 * Usage:
 *   1. Place this notify on a Montage timeline.
 *   2. Set EventTag to the desired event (e.g. Event.AnimNotify.BeginInputBuffer).
 *   3. At runtime, Notify() calls ASC->HandleGameplayEvent(EventTag).
 *   4. AbilityTasks listening for that tag via GenericGameplayEventCallbacks will fire.
 *
 * This replaces the need to create a Blueprint AnimNotify subclass for every event type.
 * One C++ class, many editor-configured instances.
 *
 * Pattern match: same owner→ASC resolution as UZZZAnimNotify_AttackTrace.
 */
UCLASS(meta = (DisplayName = "Send Gameplay Event"))
class UAnimNotify_SendGameplayEvent : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

	/** The GameplayTag to broadcast. Pick from the editor dropdown. */
	UPROPERTY(EditAnywhere, Category = "Gameplay Event")
	FGameplayTag EventTag;

	/**
	 * Optional payload object passed through to the GameplayEvent.
	 * Can be used to pass context (e.g. a damage GE class for hit events).
	 */
	UPROPERTY(EditAnywhere, Category = "Gameplay Event")
	TObjectPtr<UObject> OptionalPayload = nullptr;
};
