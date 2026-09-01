// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "ZZZDamageNumberPool.generated.h"

class AZZZDamageNumberActor;
class UZZZDamageNumberWidget;
class UWorld;

/**
 * Object pool for damage number actors (Screen-space WidgetComponents).
 *
 * Pre-creates PoolSize actors (each with one widget instance) at Initialize();
 * ShowDamageNumber() reuses a free actor instead of spawning — required to
 * avoid hit-time spawn/destroy churn during high-frequency combat.
 */
UCLASS()
class UZZZDamageNumberPool : public UObject
{
	GENERATED_BODY()

public:
	/** Pre-create the pool. Called by AZZZPlayerController in BeginPlay. */
	void Initialize(UWorld* InWorld, int32 InPoolSize, TSubclassOf<UZZZDamageNumberWidget> InWidgetClass);

	/** Display a damage number at WorldLoc (pooled). ElementTag → WBP colors it. */
	void ShowDamageNumber(const FVector& WorldLoc, float Damage, FGameplayTag ElementTag);

	/** Recycle an actor (hidden + back to the free list). */
	void Release(AZZZDamageNumberActor* Actor);

private:
	AZZZDamageNumberActor* Acquire();
	AZZZDamageNumberActor* SpawnNewActor();

	UPROPERTY(Transient)
	TObjectPtr<UWorld> World;

	UPROPERTY(Transient)
	TSubclassOf<UZZZDamageNumberWidget> WidgetClass;

	int32 MaxPoolSize = 64;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AZZZDamageNumberActor>> AllActors;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AZZZDamageNumberActor>> FreeList;
};
