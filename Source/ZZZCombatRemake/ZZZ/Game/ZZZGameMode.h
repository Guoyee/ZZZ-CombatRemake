// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ZZZGameMode.generated.h"

/**
 * Minimal GameMode for ZZZ combat.
 * Pawn-ASC (2026-08): players own their ASC on the Pawn. AZZZPlayerState is
 * kept as the future host of team-shared data (ultimate meter 喧响值, Phase 5).
 */
UCLASS()
class AZZZGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AZZZGameMode();
};
