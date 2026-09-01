// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ZZZFactionGameplayEffects.generated.h"

/**
 * Bare Infinite GE used as the carrier for faction identity tags
 * (State.Player / State.Enemy).
 *
 * The faction tag itself is added at APPLY time via FGameplayEffectSpec::DynamicGrantedTags
 * — NOT configured on the GE CDO. Rationale: the CDO is constructed during module
 * load (before ZZZCombatRemake's StartupModule), so FZZZGameplayTags::Get() would
 * still hold invalid tags there and a CDO-configured granted tag would be silently
 * dropped. Runtime tag resolution has no such timing issue.
 *
 * Tag lifecycle stays GE-managed: the tag is granted while this Infinite effect
 * is active and removed when the effect is removed / the ASC is reset or destroyed.
 */
UCLASS()
class UZZZGameplayEffect_Faction : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UZZZGameplayEffect_Faction();
};
