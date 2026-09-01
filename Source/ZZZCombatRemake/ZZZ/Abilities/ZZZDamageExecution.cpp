// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZDamageExecution.h"
#include "Attributes/ZZZAttributeSet.h"
#include "Tags/ZZZGameplayTags.h"

// ────────────────────────────────────────────────────────────
// Attribute capture definitions
// ────────────────────────────────────────────────────────────

struct FZZZDamageStatics
{
	DECLARE_ATTRIBUTE_CAPTUREDEF(Attack);
	DECLARE_ATTRIBUTE_CAPTUREDEF(Defense);
	DECLARE_ATTRIBUTE_CAPTUREDEF(IncomingDamage);
	DECLARE_ATTRIBUTE_CAPTUREDEF(Daze);

	FZZZDamageStatics()
	{
		// Snapshot=false for instant GE — no practical difference.
		// Source: attacker, Target: victim.
		DEFINE_ATTRIBUTE_CAPTUREDEF(UZZZAttributeSet, Attack,         Source, false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UZZZAttributeSet, Defense,        Target, false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UZZZAttributeSet, IncomingDamage, Target, false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UZZZAttributeSet, Daze,           Target, false);
	}
};

static const FZZZDamageStatics& DamageStatics()
{
	static FZZZDamageStatics DStatics;
	return DStatics;
}

// ────────────────────────────────────────────────────────────
// Constructor
// ────────────────────────────────────────────────────────────

UZZZDamageExecution::UZZZDamageExecution()
{
	RelevantAttributesToCapture.Add(DamageStatics().AttackDef);
	RelevantAttributesToCapture.Add(DamageStatics().DefenseDef);
	RelevantAttributesToCapture.Add(DamageStatics().IncomingDamageDef);
	RelevantAttributesToCapture.Add(DamageStatics().DazeDef);
}

// ────────────────────────────────────────────────────────────
// Execute
// ────────────────────────────────────────────────────────────

void UZZZDamageExecution::Execute_Implementation(
	const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	// ── 1. Capture Source Attack ──
	float AttackValue = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
		DamageStatics().AttackDef,
		FAggregatorEvaluateParameters(),
		AttackValue);

	// ── 2. Capture Target Defense ──
	float DefenseValue = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
		DamageStatics().DefenseDef,
		FAggregatorEvaluateParameters(),
		DefenseValue);

	// ── 3. Read SetByCaller damage values ──
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	float BaseDamage = Spec.GetSetByCallerMagnitude(
		FZZZGameplayTags::Get().Data_Damage,
		false /* bWarnIfNotFound */,
		AttackValue /* DefaultValue — fallback to raw Attack if tag missing */);

	float DazeBuildup = Spec.GetSetByCallerMagnitude(
		FZZZGameplayTags::Get().Data_Daze,
		false,
		0.0f /* Default: no Daze if tag not set */);

	// ── 4. Calculate final damage ──
	// DefenseFactor = 1 - Defense/(Defense+500)
	// At 0 Defense: 1.0x, at 500 Defense: 0.5x, at ∞: 0.0x
	const float DefenseFactor = FMath::Max(0.0f, 1.0f - DefenseValue / (DefenseValue + 500.0f));
	const float FinalDamage = FMath::Max(0.0f, BaseDamage * DefenseFactor);
	// ── 5. Apply Daze (absolute value, independent of damage) ──

	// IncomingDamage (Meta — consumed by PostGameplayEffectExecute → Health deduction)
	if (FinalDamage > 0.0f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
			UZZZAttributeSet::GetIncomingDamageAttribute(),
			EGameplayModOp::Additive,
			FinalDamage,
			FActiveGameplayEffectHandle()));
	}

	// Daze (buildup — checked by PostGameplayEffectExecute → stun detection)
	if (DazeBuildup > 0.0f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
			UZZZAttributeSet::GetDazeAttribute(),
			EGameplayModOp::Additive,
			DazeBuildup,
			FActiveGameplayEffectHandle()));
	}
}
