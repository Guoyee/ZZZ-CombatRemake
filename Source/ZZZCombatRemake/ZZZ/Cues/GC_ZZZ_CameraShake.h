// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "GC_ZZZ_CameraShake.generated.h"

class UCameraShakeBase;

/**
 * GameplayCue that plays a short camera shake — hit-frame feedback
 * (Phase 3 Task 5a). NOT used for perfect dodge (2026-08-16): the slow
 * motion itself is the dodge reward.
 *
 * Triggered from code:
 *   - UZZZAnimNotify_AttackTrace (hit frame): on the target's ASC, per-notify
 *     instance (bApplyCameraShake switch). The shake resolves the local
 *     player's camera from the world, so it works for hits in either
 *     direction. RawMagnitude carries the hit's BaseDamage (reserved for
 *     intensity scaling).
 *
 * Stateless by design (like GC_ZZZ_DamageNumber / GC_ZZZ_EnemyAttackWarning):
 * just starts a shake on the local player's camera — no restore step.
 *
 * Shake tiers (2026-08-16): three Blueprint children, ONE per tier tag —
 * the CueManager executes ALL handlers registered for a tag, so two assets
 * sharing a tag double-fire and fight over the shake (bSingleInstance
 * restarts instead of stacking):
 *   GC_ZZZ_CameraShake_Low  — GameplayCueTag = GameplayCue.ZZZ.CameraShake.Low
 *   GC_ZZZ_CameraShake_Mid  — GameplayCueTag = GameplayCue.ZZZ.CameraShake.Mid
 *   GC_ZZZ_CameraShake_High — GameplayCueTag = GameplayCue.ZZZ.CameraShake.High
 * Each sets:
 *   - CameraShakeClass = BP_HitShake (the SINGLE shake asset — shaking is
 *     always the local player's camera, so no per-faction shake assets)
 *   - ShakeScale       = per-tier intensity multiplier
 */
UCLASS()
class UGC_ZZZ_CameraShake : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	UGC_ZZZ_CameraShake();

	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;

	virtual bool OnExecute_Implementation(
		AActor* MyTarget,
		const FGameplayCueParameters& Parameters) const override;

	/** Shake to play. Reuses BP_CameraShake_Hit_Player from Variant_Combat. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Cue")
	TSubclassOf<UCameraShakeBase> CameraShakeClass;

	/** Intensity multiplier (passed to ClientStartCameraShake). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Cue")
	float ShakeScale = 1.0f;
};
