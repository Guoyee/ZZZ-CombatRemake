// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "GC_ZZZ_PlaySound.generated.h"

class USoundBase;

/**
 * GameplayCue that plays a one-shot 3D sound — montage-frame-triggered
 * attack audio (swing whoosh / heavy boom / sheathe).
 *
 * Triggered from montages (zero C++ on the montage side):
 *   Swing frame → engine built-in "GameplayCue (Burst)" notify →
 *   GameplayCue.ZZZ.Sound.<Attack>.<Hit> → this cue → SpawnSoundAttached.
 *
 * One-shot by design: clips are short (< 0.5s), triggered at the exact
 * montage frame they were recorded against (per-attack split clips — NOT a
 * whole-attack loop). Interruption needs no lifecycle management:
 *   - break before the frame → notify never fired → silent
 *   - break after the frame → short clip plays out (matches ZZZ feel)
 * Audio does NOT follow CustomTimeDilation, so split clips keep alignment
 * under slow motion / hitstop where a full-attack clip would run ahead of
 * the animation.
 *
 * One tag per audio file per GCN asset: the CueManager executes ALL
 * handlers registered for a tag, so sharing a tag double-fires (see
 * GC_ZZZ_CameraShake tier note). Create one Blueprint child per cue tag
 * (child sets GameplayCueTag + Sound).
 */
UCLASS()
class UGC_ZZZ_PlaySound : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	UGC_ZZZ_PlaySound();

	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;

	virtual bool OnExecute_Implementation(
		AActor* MyTarget,
		const FGameplayCueParameters& Parameters) const override;

	/** Sound to play. Set on the Blueprint child (one child per cue tag). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Cue")
	TObjectPtr<USoundBase> Sound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Cue",
		meta = (ClampMin = "0.0"))
	float VolumeMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Cue",
		meta = (ClampMin = "0.01"))
	float PitchMultiplier = 1.0f;

	/**
	 * Socket on the skeletal mesh to attach the sound to (spatialized at the
	 * weapon). NAME_None = root component.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Cue")
	FName AttachSocket;
};
