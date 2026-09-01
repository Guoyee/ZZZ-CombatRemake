// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZDamageNumberActor.h"

#include "Components/WidgetComponent.h"
#include "TimerManager.h"
#include "ZZZDamageNumberPool.h"
#include "ZZZDamageNumberWidget.h"

AZZZDamageNumberActor::AZZZDamageNumberActor()
{
	PrimaryActorTick.bCanEverTick = false;

	DamageWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("DamageWidget"));
	RootComponent = DamageWidgetComponent;

	// Screen space: always camera-facing, occlusion-free, UI-crisp.
	// The widget is anchored at this actor's world location automatically.
	DamageWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	DamageWidgetComponent->SetDrawSize(FVector2D(160.0f, 48.0f));
	DamageWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DamageWidgetComponent->SetWindowFocusable(false);
}

void AZZZDamageNumberActor::SetDamageWidget(UZZZDamageNumberWidget* Widget)
{
	DamageWidget = Widget;
	if (DamageWidgetComponent)
	{
		DamageWidgetComponent->SetWidget(Widget);
	}
}

void AZZZDamageNumberActor::Activate(const FVector& WorldLoc, float Damage, FGameplayTag ElementTag)
{
	// Random horizontal scatter so consecutive hits don't stack on one pixel.
	const float ScatterX = FMath::FRandRange(-ScatterRadius, ScatterRadius);
	const float ScatterY = FMath::FRandRange(-ScatterRadius, ScatterRadius);
	SetActorLocation(WorldLoc + FVector(ScatterX, ScatterY, 0.0f));

	SetActorHiddenInGame(false);
	if (DamageWidget)
	{
		DamageWidget->SetDamageValue(Damage, ElementTag);
	}

	// Hard recycle deadline — not driven by the widget animation (see header).
	GetWorldTimerManager().SetTimer(
		LifetimeTimer, this, &AZZZDamageNumberActor::OnLifetimeExpired, MaxLifetime, false);
}

void AZZZDamageNumberActor::OnLifetimeExpired()
{
	ReleaseToPool();
}

void AZZZDamageNumberActor::ReleaseToPool()
{
	GetWorldTimerManager().ClearTimer(LifetimeTimer);
	SetActorHiddenInGame(true);
	if (OwningPool)
	{
		OwningPool->Release(this);
	}
}
