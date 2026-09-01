// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "GC_ZZZ_DamageNumber.generated.h"

/**
 * GameplayCue that shows a floating damage number (pooled Screen-space widget).
 *
 * Triggered from PostGameplayEffectExecute when damage is applied; routes to
 * the local player's UZZZDamageNumberPool. Tag: GameplayCue.ZZZ.DamageNumber
 */
UCLASS()
class UGC_ZZZ_DamageNumber : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	UGC_ZZZ_DamageNumber();

	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;

	virtual bool OnExecute_Implementation(
		AActor* MyTarget,
		const FGameplayCueParameters& Parameters) const override;
};
