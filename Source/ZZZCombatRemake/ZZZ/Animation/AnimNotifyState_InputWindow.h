// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "AnimNotifyState_InputWindow.generated.h"

/**
 * AnimNotifyState that manages the input buffer window tag (Effect.Input.CanBuffer).
 *
 * Drag onto the montage timeline where buffering should be allowed.
 * NotifyBegin → ASC->AddLooseGameplayTag(Effect.Input.CanBuffer)
 * NotifyEnd   → ASC->RemoveLooseGameplayTag(Effect.Input.CanBuffer)
 */
UCLASS(meta = (DisplayName = "Input Buffer Window"))
class UAnimNotifyState_InputWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	UPROPERTY(EditAnywhere, Category = "Input Window")
	FGameplayTag WindowTag;
};
