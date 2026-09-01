// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZDamageNumberPool.h"

#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "ZZZDamageNumberActor.h"
#include "ZZZDamageNumberWidget.h"

void UZZZDamageNumberPool::Initialize(UWorld* InWorld, int32 InPoolSize, TSubclassOf<UZZZDamageNumberWidget> InWidgetClass)
{
	World = InWorld;
	WidgetClass = InWidgetClass
		? InWidgetClass
		: TSubclassOf<UZZZDamageNumberWidget>(UZZZDamageNumberWidget::StaticClass());

	AllActors.Reset();
	FreeList.Reset();

	for (int32 i = 0; i < InPoolSize; ++i)
	{
		if (AZZZDamageNumberActor* Actor = SpawnNewActor())
		{
			FreeList.Add(Actor);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[DamageNumberPool] initialized with %d actors"), AllActors.Num());
}

void UZZZDamageNumberPool::ShowDamageNumber(const FVector& WorldLoc, float Damage, FGameplayTag ElementTag)
{
	AZZZDamageNumberActor* Actor = Acquire();
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[DamageNumberPool] pool exhausted (max %d), dropping damage number."), MaxPoolSize);
		return;
	}
	Actor->Activate(WorldLoc, Damage, ElementTag);
}

void UZZZDamageNumberPool::Release(AZZZDamageNumberActor* Actor)
{
	if (Actor)
	{
		Actor->SetActorHiddenInGame(true);
		FreeList.AddUnique(Actor);
	}
}

AZZZDamageNumberActor* UZZZDamageNumberPool::Acquire()
{
	if (FreeList.Num() > 0)
	{
		return FreeList.Pop();
	}

	// Grow on demand (bounded).
	if (AllActors.Num() < MaxPoolSize)
	{
		if (AZZZDamageNumberActor* Actor = SpawnNewActor())
		{
			return Actor;
		}
	}
	return nullptr;
}

AZZZDamageNumberActor* UZZZDamageNumberPool::SpawnNewActor()
{
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AZZZDamageNumberActor* Actor = World->SpawnActor<AZZZDamageNumberActor>(
		AZZZDamageNumberActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (!Actor)
	{
		return nullptr;
	}

	// One widget instance per actor, created once here — never at hit time.
	if (UZZZDamageNumberWidget* Widget = CreateWidget<UZZZDamageNumberWidget>(World, WidgetClass))
	{
		Actor->SetDamageWidget(Widget);
	}

	Actor->SetActorHiddenInGame(true);
	Actor->OwningPool = this;
	AllActors.Add(Actor);
	return Actor;
}
