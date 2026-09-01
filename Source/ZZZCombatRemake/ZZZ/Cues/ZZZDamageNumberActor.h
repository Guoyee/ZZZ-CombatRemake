// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "ZZZDamageNumberActor.generated.h"

class UWidgetComponent;
class UZZZDamageNumberWidget;
class UZZZDamageNumberPool;

/**
 * Pooled floating damage number (Screen-space WidgetComponent).
 *
 * Owns a single UWidgetComponent in EWidgetSpace::Screen — the engine renders
 * the widget as screen-space UI anchored at this actor's world location, so
 * it is always camera-facing, occlusion-free and crisply scaled (no per-frame
 * rotation code needed).
 *
 * Lifecycle is timer-driven: the pool calls Activate() on hit, which starts a
 * hard-capped lifetime timer (MaxLifetime). When it expires, OnLifetimeExpired
 * → ReleaseToPool() recycles the actor. The recycle is deliberately NOT driven
 * by the WBP's animation-finished callback — a forgotten callback previously
 * leaked the whole pool (all 64 actors stuck out, every later hit dropped) —
 * so the pool can never starve.
 */
UCLASS()
class AZZZDamageNumberActor : public AActor
{
	GENERATED_BODY()

public:
	AZZZDamageNumberActor();

	/** Pool hook: position the anchor, forward value + element tag to the widget. */
	void Activate(const FVector& WorldLoc, float Damage, FGameplayTag ElementTag);

	/** Attach the pooled widget instance (once, at pool creation). */
	void SetDamageWidget(UZZZDamageNumberWidget* Widget);

	/** Random horizontal scatter (cm) applied to the anchor per activation. */
	UPROPERTY(EditDefaultsOnly, Category = "ZZZ|DamageNumber")
	float ScatterRadius = 30.0f;

	/**
	 * Hard lifetime cap per activation (gameplay anims never exceed 1.5s).
	 * Recycle happens on this timer — NOT on a widget animation callback — so
	 * a broken WBP can never leak the pool.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "ZZZ|DamageNumber")
	float MaxLifetime = 1.5f;

protected:
	/** Timer callback: recycle this actor when MaxLifetime expires. */
	void OnLifetimeExpired();

	/** Return this actor to the owning pool (hidden). */
	void ReleaseToPool();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent> DamageWidgetComponent;

	UPROPERTY(Transient)
	TObjectPtr<UZZZDamageNumberWidget> DamageWidget;

	/** Set by the pool; used to recycle this actor. */
	UPROPERTY(Transient)
	TObjectPtr<UZZZDamageNumberPool> OwningPool;

	/** Active only between Activate() and ReleaseToPool(). */
	FTimerHandle LifetimeTimer;

	friend class UZZZDamageNumberPool;
};
