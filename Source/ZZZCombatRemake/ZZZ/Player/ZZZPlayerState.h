// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "ZZZPlayerState.generated.h"

/**
 * PlayerState for ZZZ combat.
 *
 * Pawn-ASC (2026-08-01): does NOT host the AbilitySystemComponent /
 * AttributeSet — those live on each squad member's Pawn for independent
 * HP / resource / cooldown isolation (see AZZZCharacter).
 *
 * Future role (Phase 5): host TEAM-shared data that must survive pawn
 * switches — the ultimate meter (喧响值 / Decibel) shared across the squad.
 * A PlayerState persists for the whole match regardless of which pawn is
 * currently possessed, which is exactly what the shared meter needs.
 */
UCLASS()
class AZZZPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AZZZPlayerState();
};
