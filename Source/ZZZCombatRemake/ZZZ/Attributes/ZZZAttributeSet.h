// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "ZZZAttributeSet.generated.h"

// Standard GAS attribute accessor macros
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * AttributeSet for ZZZ combat system.
 *
 * Phase 2 MVP attributes:
 *   Vital:    Health, MaxHealth, Daze, MaxDaze
 *   Primary:  Attack, Defense
 *   Meta:     IncomingDamage (ExecCalc output → PostGEExecute consumption)
 *
 * Deferred to Phase 3+: Energy, MaxEnergy, AnomalyMastery, AnomalyProficiency,
 * AnomalyBuildup. (TimeDilation added in Phase 3 Task 0 — GE bridge in Task 3.)
 */
UCLASS()
class UZZZAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UZZZAttributeSet();

	// === Core Vital Attributes ===

	UPROPERTY(BlueprintReadWrite, Category = "Attributes|Vital")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UZZZAttributeSet, Health);

	UPROPERTY(BlueprintReadWrite, Category = "Attributes|Vital")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UZZZAttributeSet, MaxHealth);

	// === Daze / Stun ===

	UPROPERTY(BlueprintReadWrite, Category = "Attributes|Vital")
	FGameplayAttributeData Daze;
	ATTRIBUTE_ACCESSORS(UZZZAttributeSet, Daze);

	UPROPERTY(BlueprintReadWrite, Category = "Attributes|Vital")
	FGameplayAttributeData MaxDaze;
	ATTRIBUTE_ACCESSORS(UZZZAttributeSet, MaxDaze);

	// === Primary Combat Stats ===

	/** Attack power — captured by ExecCalc from the Source (attacker). */
	UPROPERTY(BlueprintReadWrite, Category = "Attributes|Primary")
	FGameplayAttributeData Attack;
	ATTRIBUTE_ACCESSORS(UZZZAttributeSet, Attack);

	/** Defense — captured by ExecCalc from the Target (victim). */
	UPROPERTY(BlueprintReadWrite, Category = "Attributes|Primary")
	FGameplayAttributeData Defense;
	ATTRIBUTE_ACCESSORS(UZZZAttributeSet, Defense);

	// === Meta Attribute ===

	/**
	 * Damage pipeline intermediate value.
	 * Written by ZZZDamageExecution (ExecCalc), consumed and zeroed in
	 * PostGameplayEffectExecute. Never persists across frames.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Meta")
	FGameplayAttributeData IncomingDamage;
	ATTRIBUTE_ACCESSORS(UZZZAttributeSet, IncomingDamage);

	// === Time Bridge (Phase 3) ===

	/**
	 * Mirror of AActor::CustomTimeDilation, driven by GE_SlowMotion (Duration GE).
	 * NOT bridged in PostGameplayEffectExecute (M1: Duration GEs don't trigger it) —
	 * AZZZCombatEnemy binds GetGameplayAttributeValueChangeDelegate in Task 3.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Attributes|Time")
	FGameplayAttributeData TimeDilation;
	ATTRIBUTE_ACCESSORS(UZZZAttributeSet, TimeDilation);

	// === Attribute Change Callbacks ===

	/** Clamp Health→[0, MaxHealth], Daze→[0, MaxDaze]. */
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	/**
	 * Consumes IncomingDamage (Meta Attribute): reads damage → subtracts from Health → zeroes.
	 * Also handles death detection (Event.Combat.Elimination) and stun detection (Event.Combat.Stun).
	 */
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

private:
	// Prevent repeated event broadcasting (single-player only — no replication needed)
	bool bIsDead = false;
	bool bIsStunned = false;
};
