// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "ZZZDamageExecution.generated.h"

/**
 * Unified damage execution calculation for ZZZ combat.
 *
 * Captures:
 *   Source: Attack
 *   Target: Defense
 *
 * Reads SetByCaller:
 *   Data.Damage  — base damage before defense scaling
 *   Data.Daze    — absolute daze buildup (independent of damage)
 *
 * Writes (output modifiers):
 *   Target IncomingDamage (+FinalDamage)  — consumed by PostGameplayEffectExecute
 *   Target Daze           (+DazeBuildup)  — stun threshold detection
 *
 * Formulas:
 *   DefenseFactor = Max(0, 1 - Defense / (Defense + 500))
 *   FinalDamage   = BaseDamage * DefenseFactor
 *   Daze is applied as-is from SetByCaller (flat value, not scaled by damage)
 */
UCLASS()
class UZZZDamageExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UZZZDamageExecution();

	virtual void Execute_Implementation(
		const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
