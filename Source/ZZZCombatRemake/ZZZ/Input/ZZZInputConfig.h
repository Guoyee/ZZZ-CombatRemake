// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ZZZInputConfig.generated.h"

class UInputAction;

/**
 * A single InputAction → GameplayTag binding.
 *
 * Example:
 *   InputAction = IA_ZZZAttack
 *   InputTag    = Input.Attack
 */
USTRUCT(BlueprintType)
struct FZZZInputAction
{
	GENERATED_BODY()

	/** The Enhanced Input Action (e.g. IA_ZZZAttack). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<const UInputAction> InputAction = nullptr;

	/** The GameplayTag to broadcast when this input is pressed (e.g. Input.Attack). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag InputTag;

	/**
	 * Trigger on Started (single fire) instead of Triggered (fires every frame
	 * while held). Required for one-shot actions like Dodge / Switch — Triggered
	 * would re-fire for the whole duration of the press.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bTriggerOnStarted = false;
};

/**
 * Lyra-style DataAsset that maps Enhanced Input Actions to GameplayTags.
 *
 * The Character iterates this array in SetupPlayerInputComponent and binds
 * each InputAction's Triggered/Completed events to ASC GameplayEvent forwarding.
 *
 * Create one instance at Content/ZZZ/Data/DA_ZZZInputConfig.
 */
UCLASS(BlueprintType)
class UZZZInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/** All ability-related input bindings. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input",
		meta = (TitleProperty = "InputTag"))
	TArray<FZZZInputAction> AbilityInputActions;
};
