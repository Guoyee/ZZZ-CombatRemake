// Copyright Epic Games, Inc. All Rights Reserved.

#include "GC_ZZZ_CameraShake.h"

#include "Camera/CameraShakeBase.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Tags/ZZZGameplayTags.h"
#include "ZZZCombatRemake.h"

UGC_ZZZ_CameraShake::UGC_ZZZ_CameraShake()
{
	// Single source of truth — registered in DefaultGameplayTags.ini
	// (engine-load time, before any CDO ctor) with a defensive manager lookup:
	// a CDO constructor can run before native tag registration, and a BP child
	// inherits this value — an empty tag here silently kills registration.
	GameplayCueTag = FZZZGameplayTags::Get().GameplayCue_ZZZ_CameraShake;
	if (!GameplayCueTag.IsValid())
	{
		GameplayCueTag = FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.CameraShake"), false);
	}

	// GameplayCueName is the AssetRegistrySearchable mirror of GameplayCueTag
	// (see GameplayCueNotify_Static.h). The CueManager's scan path
	// (BuildCuesToAddToGlobalSet) reads GameplayCueName from the FAssetData —
	// NOT the CDO — so a None here means the asset is skipped at scan time and
	// the cue tag maps to INDEX_NONE ("unmapped" in
	// GameplayCue.PrintGameplayCueNotifyMap), silently dropping every execute.
	// Keep both in sync (R20 — see design doc 8.11).
	GameplayCueName = GameplayCueTag.GetTagName();

	// Native cue classes are only reliably picked up by the CueManager when
	// marked as overrides — otherwise the tag may resolve to nothing and
	// ExecuteGameplayCue silently no-ops. (UE 5.8 renamed the property:
	// IsOverride, not bIsOverride.)
	IsOverride = true;
}

void UGC_ZZZ_CameraShake::PostInitProperties()
{
	Super::PostInitProperties();

	// The engine's asset-name derivation (UGameplayCueNotify_Static::
	// PostInitProperties → DeriveGameplayCueTagFromAssetName) blanks
	// GameplayCueName whenever the derived tag is not registered — which is
	// always the case here ("GC_CameraShake" derives to the unregistered
	// "GameplayCue.CameraShake", the derivation fails, and the tag falls back
	// to the parent's while GameplayCueName is left as None). The CueManager
	// scan reads GameplayCueName from the FAssetData, so a None here makes the
	// cue permanently unmapped. Re-sync it.
	GameplayCueName = GameplayCueTag.GetTagName();
}

void UGC_ZZZ_CameraShake::PostLoad()
{
	Super::PostLoad();

	// The serialized GameplayCueName from a pre-fix save is None, and the
	// deserialized value overwrites whatever PostInitProperties set. Re-sync
	// after deserialization so the in-memory CDO always carries the tag
	// mirror (a resave then persists it — see Serialize).
	GameplayCueName = GameplayCueTag.GetTagName();
}

void UGC_ZZZ_CameraShake::Serialize(FArchive& Ar)
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

bool UGC_ZZZ_CameraShake::OnExecute_Implementation(
	AActor* MyTarget,
	const FGameplayCueParameters& Parameters) const
{
	if (!CameraShakeClass)
	{
		UE_LOG(LogZZZCombatRemake, Warning,
			TEXT("GC_ZZZ_CameraShake: CameraShakeClass not configured on %s — set it on the Blueprint child"),
			*GetNameSafe(this));
		return false;
	}

	UWorld* World = MyTarget ? MyTarget->GetWorld() : GetWorld();
	if (!World)
	{
		UE_LOG(LogZZZCombatRemake, Warning,
			TEXT("GC_ZZZ_CameraShake: no World (target=%s)"), *GetNameSafe(MyTarget));
		return false;
	}

	// Single-player: shake the local player's camera. The cue fires on the
	// player's own ASC (perfect dodge), so MyTarget is the player character —
	// but resolve from the world so it also works when fired on an enemy.
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		UE_LOG(LogZZZCombatRemake, Verbose,
			TEXT("GC_ZZZ_CameraShake: shake '%s' scale=%.2f on %s"),
			*CameraShakeClass->GetName(), ShakeScale, *GetNameSafe(PC->GetPawn()));
		PC->ClientStartCameraShake(CameraShakeClass, ShakeScale);
		return true;
	}

	UE_LOG(LogZZZCombatRemake, Warning,
		TEXT("GC_ZZZ_CameraShake: no PlayerController in world %s"), *GetNameSafe(World));
	return false;
}
