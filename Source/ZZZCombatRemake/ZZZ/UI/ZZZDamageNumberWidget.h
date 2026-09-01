// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "ZZZDamageNumberWidget.generated.h"

/**
 * Base class for floating damage numbers.
 *
 * Division of labor:
 *   C++ — passes semantic data only (damage value + element tag). Lifetime is
 *         a hard cap managed by the owning AZZZDamageNumberActor (timer), so
 *         the WBP never has to report back.
 *   BP (WBP_DamageNumber) — ALL presentation: text formatting, element colors,
 *         float/fade animation. Implements BP_OnDamageShown.
 */
UCLASS()
class UZZZDamageNumberWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** C++ entry point: forward damage + element tag to the BP presentation. */
	UFUNCTION(BlueprintCallable, Category = "ZZZ|DamageNumber")
	void SetDamageValue(float Damage, FGameplayTag ElementTag);

	/**
	 * Implemented in WBP_DamageNumber: format the text, pick the color
	 * (heal=green, element via GetElementColor), play the FloatFade animation.
	 * The actor recycles itself on a hard MaxLifetime timer — no callback needed.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ZZZ|DamageNumber")
	void BP_OnDamageShown(float Damage, FGameplayTag ElementTag);

	/**
	 * Element → color mapping (callable from BP). Phase 4 extension point:
	 * map Element.Fire / Ice / Electric / Ether to distinct colors here —
	 * the rest of the pipeline stays untouched.
	 */
	UFUNCTION(BlueprintCallable, Category = "ZZZ|DamageNumber")
	static FLinearColor GetElementColor(FGameplayTag ElementTag);
};
