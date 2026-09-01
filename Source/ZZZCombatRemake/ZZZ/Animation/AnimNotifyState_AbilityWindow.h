// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "AnimNotifyState_AbilityWindow.generated.h"

/**
 * AnimNotifyState that manages a GameplayTag on the owner's ASC for its duration.
 *
 * Primary use case: Marking the "combo window" in a melee attack's Recovery section.
 *
 *   NotifyBegin() → ASC->AddLooseGameplayTag(WindowTag)
 *   NotifyEnd()   → ASC->RemoveLooseGameplayTag(WindowTag)
 *
 * The default WindowTag is "Effect.Ability.CanCombo", but can be changed
 * for other window types (e.g. "Effect.Ability.CanParry").
 *
 * Usage in Montage editor:
 *   Drag this NotifyState onto the Recovery section. The blue bar = combo window.
 *
 * On the C++ side, UAbilityTask_WaitCombo checks HasMatchingGameplayTag(WindowTag) each tick.
 */
UCLASS(meta = (DisplayName = "Ability Window"))
class UAnimNotifyState_AbilityWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

	/**
	 * The GameplayTag to add/remove for the duration of this notify state.
	 * Default: "Effect.Ability.CanCombo".
	 * Change this to create different window types (parry, dodge, etc.).
	 */
	UPROPERTY(EditAnywhere, Category = "Ability Window")
	FGameplayTag WindowTag;
};
