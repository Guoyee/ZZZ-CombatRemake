// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZFactionGameplayEffects.h"

UZZZGameplayEffect_Faction::UZZZGameplayEffect_Faction()
{
	// No tag configuration on the CDO on purpose — see header comment.
	DurationPolicy = EGameplayEffectDurationType::Infinite;
}
