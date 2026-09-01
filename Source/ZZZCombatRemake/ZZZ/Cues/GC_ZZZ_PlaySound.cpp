// Copyright Epic Games, Inc. All Rights Reserved.

#include "GC_ZZZ_PlaySound.h"

#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Tags/ZZZGameplayTags.h"
#include "ZZZCombatRemake.h"

UGC_ZZZ_PlaySound::UGC_ZZZ_PlaySound()
{
	// Default cue tag = first attack sound (a placeholder — every Blueprint
	// child MUST override GameplayCueTag with its own tag + Sound; the
	// CueManager executes all handlers for a tag, so children must not share
	// tags either). Same pattern as GC_ZZZ_CameraShake's base default.
	GameplayCueTag = FZZZGameplayTags::Get().GameplayCue_ZZZ_Sound_BasicAttack01_Swing1;
	if (!GameplayCueTag.IsValid())
	{
		GameplayCueTag = FGameplayTag::RequestGameplayTag(
			FName("GameplayCue.ZZZ.Sound.BasicAttack01.Swing1"), false);
	}

	// GameplayCueName is the AssetRegistrySearchable mirror of GameplayCueTag
	// (see GameplayCueNotify_Static.h). The CueManager's scan path
	// (BuildCuesToAddToGlobalSet) reads GameplayCueName from the FAssetData —
	// NOT the CDO — so a None here means the asset is skipped at scan time and
	// the cue tag maps to INDEX_NONE ("unmapped"), silently dropping every
	// execute. Keep both in sync (R20 — see design doc 8.11).
	GameplayCueName = GameplayCueTag.GetTagName();

	// Native cue classes are only reliably picked up by the CueManager when
	// marked as overrides — otherwise the tag may resolve to nothing and
	// ExecuteGameplayCue silently no-ops. (UE 5.8 renamed the property:
	// IsOverride, not bIsOverride.)
	IsOverride = true;
}

void UGC_ZZZ_PlaySound::PostInitProperties()
{
	Super::PostInitProperties();

	// The engine's asset-name derivation (UGameplayCueNotify_Static::
	// PostInitProperties → DeriveGameplayCueTagFromAssetName) blanks
	// GameplayCueName whenever the derived tag is not registered — which is
	// always the case here (the asset name derives to an unregistered tag,
	// the derivation fails, and the tag falls back to the parent's while
	// GameplayCueName is left as None). The CueManager scan reads
	// GameplayCueName from the FAssetData, so a None here makes the cue
	// permanently unmapped. Re-sync it.
	GameplayCueName = GameplayCueTag.GetTagName();
}

void UGC_ZZZ_PlaySound::PostLoad()
{
	Super::PostLoad();

	// The serialized GameplayCueName from a pre-fix save is None, and the
	// deserialized value overwrites whatever PostInitProperties set. Re-sync
	// after deserialization so the in-memory CDO always carries the tag
	// mirror (a resave then persists it — see Serialize).
	GameplayCueName = GameplayCueTag.GetTagName();
}

void UGC_ZZZ_PlaySound::Serialize(FArchive& Ar)
{
	if (Ar.IsSaving())
	{
		// Same derivation hazard on save: the engine's Serialize derives before
		// writing the property stream and would persist GameplayCueName = None.
		// Skip the engine's derive wrapper on save (this class's tag is set
		// explicitly by the ctor — no derivation needed) and persist the
		// registry mirror so the AssetRegistry carries the cue tag.
		GameplayCueName = GameplayCueTag.GetTagName();
		UObject::Serialize(Ar);
		return;
	}

	Super::Serialize(Ar);

	if (Ar.IsLoading())
	{
		// The engine's load-path derivation also blanks GameplayCueName;
		// restore it after the derive ran.
		GameplayCueName = GameplayCueTag.GetTagName();
	}
}

bool UGC_ZZZ_PlaySound::OnExecute_Implementation(
	AActor* MyTarget,
	const FGameplayCueParameters& Parameters) const
{
	if (!Sound)
	{
		UE_LOG(LogZZZCombatRemake, Warning,
			TEXT("GC_ZZZ_PlaySound: Sound not configured on %s — set it on the Blueprint child"),
			*GetNameSafe(this));
		return false;
	}

	if (!MyTarget)
	{
		UE_LOG(LogZZZCombatRemake, Warning,
			TEXT("GC_ZZZ_PlaySound: no target (cue=%s)"), *GetNameSafe(this));
		return false;
	}

	// Attach to the weapon socket when configured (falls back to the root if
	// the socket is missing), so the sound spatializes with the swing.
	USceneComponent* AttachTo = MyTarget->GetRootComponent();
	if (AttachSocket != NAME_None)
	{
		if (USkeletalMeshComponent* SkelMesh = MyTarget->FindComponentByClass<USkeletalMeshComponent>())
		{
			AttachTo = SkelMesh;
		}
	}

	UGameplayStatics::SpawnSoundAttached(
		Sound, AttachTo, AttachSocket,
		FVector::ZeroVector, EAttachLocation::SnapToTarget,
		/*bStopWhenAttachedToDestroyed=*/true,
		VolumeMultiplier, PitchMultiplier,
		/*StartTime=*/0.0f,
		/*AttenuationSettings=*/nullptr,
		/*ConcurrencySettings=*/nullptr,
		/*bAutoDestroy=*/true);
	return true;
}
