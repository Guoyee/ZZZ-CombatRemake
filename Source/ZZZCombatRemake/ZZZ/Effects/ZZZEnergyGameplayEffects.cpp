// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZEnergyGameplayEffects.h"

#include "Attributes/ZZZAttributeSet.h"

UZZZGameplayEffect_EnergyDelta::UZZZGameplayEffect_EnergyDelta()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// Additive modifier whose magnitude is read from the spec's SetByCaller
	// map at evaluation time (Data.Energy). UE 5.8: FGameplayEffectModifierMagnitude
	// members are protected — the FSetByCallerFloat constructor is the only
	// way to build a SetByCaller magnitude in C++ (members were public pre-5.8;
	// old `M.SetByCallerMagnitude.DataTag = ...` no longer compiles).
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UZZZAttributeSet::GetEnergyAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat SetByCaller;
	// ini-registered tag (DefaultGameplayTags.ini) — required, see header.
	// ErrorIfNotFound=false: the fallback is a silent zero magnitude, which is
	// why the ini entry carries the DevComment explaining the dependency.
	SetByCaller.DataTag = FGameplayTag::RequestGameplayTag(FName("Data.Energy"), false);
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

	Modifiers.Add(Modifier);
}
