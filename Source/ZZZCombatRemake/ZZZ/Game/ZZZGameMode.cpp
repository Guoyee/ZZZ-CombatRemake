// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZGameMode.h"
#include "ZZZPlayerState.h"

AZZZGameMode::AZZZGameMode()
{
	// Native GameplayTags are registered in FZZZCombatRemakeModule::StartupModule()
	// (module load time, before any asset constructor) — not here.

	// Pawn-ASC (2026-08): the ASC lives on the Pawn, but AZZZPlayerState is
	// still wired as the future host of team-shared data (ultimate meter
	// 喧响值 / Decibel, Phase 5) — it survives pawn switches by design.
	PlayerStateClass = AZZZPlayerState::StaticClass();
}
