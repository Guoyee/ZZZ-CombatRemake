// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ZZZEnergyGameplayEffects.generated.h"

/**
 * Energy-delta carrier (2026-09-03) — every Energy change goes through this
 * one Instant GE: hit gain (AttributeSet), natural regen (character world
 * timer), enhanced-special cost (ability). Magnitude is SetByCaller
 * (Data.Energy) so the three call sites set their own value:
 *
 *   MakeOutgoingSpec → Spec.Data->SetSetByCallerMagnitude(Data.Energy, Delta)
 *                      → ApplyGameplayEffectSpecToSelf   // 幅值必须 Apply 前设
 *
 * Why a C++ carrier and not a BP asset: all call sites are C++ hardcoded
 * logic paths with no configuration host (same rationale as the status
 * carriers in ZZZStatusGameplayEffects.h) — the VARIABLE magnitude rides the
 * SetByCaller value, not the CDO.
 *
 * Why SetByCaller and not SetEnergy() directly: direct attribute sets bypass
 * the aggregator, so a future energy bar bound to
 * GetGameplayAttributeValueChangeDelegate (TimeDilation bridge pattern) would
 * never see the change. All Energy changes go through GE (clamped in
 * PreAttributeChange).
 *
 * ⚠ CDO timing (CLAUDE.md 规则 2): the CDO is constructed during module load,
 * before native tags register — the modifier's FSetByCallerFloat.DataTag is
 * resolved with RequestGameplayTag("Data.Energy", false) here and therefore
 * REQUIRES the tag in Config/DefaultGameplayTags.ini. A missing ini entry does
 * not fail compilation — the modifier silently evaluates to 0 with an Error
 * log on every apply.
 */
UCLASS()
class UZZZGameplayEffect_EnergyDelta : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UZZZGameplayEffect_EnergyDelta();
};
