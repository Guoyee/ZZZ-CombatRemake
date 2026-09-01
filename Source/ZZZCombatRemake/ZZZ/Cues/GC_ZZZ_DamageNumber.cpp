// Copyright Epic Games, Inc. All Rights Reserved.

#include "GC_ZZZ_DamageNumber.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Tags/ZZZGameplayTags.h"
#include "ZZZDamageNumberPool.h"
#include "ZZZDamageNumberWidget.h"
#include "ZZZPlayerController.h"

UGC_ZZZ_DamageNumber::UGC_ZZZ_DamageNumber()
{
	// Single source of truth — registered in DefaultGameplayTags.ini
	// (engine-load time, before any CDO ctor) with a defensive manager lookup:
	// a CDO constructor can run before native tag registration, and a BP child
	// inherits this value — an empty tag here silently kills registration.
	GameplayCueTag = FZZZGameplayTags::Get().GameplayCue_ZZZ_DamageNumber;
	if (!GameplayCueTag.IsValid())
	{
		GameplayCueTag = FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.DamageNumber"), false);
	}

	// Native cue classes are only reliably picked up by the CueManager when
	// marked as overrides — otherwise the tag may resolve to nothing and
	// ExecuteGameplayCue silently no-ops. (UE 5.8 renamed the property:
	// IsOverride, not bIsOverride.)
	IsOverride = true;

	// GameplayCueName is the AssetRegistrySearchable mirror of GameplayCueTag
	// (R20 — see design doc 8.11): the CueManager's scan reads GameplayCueName
	// from the FAssetData, NOT the CDO, and the engine's asset-name derivation
	// blanks it on construct/save. Keep both in sync — a resave of the
	// Blueprint child would otherwise persist None and the cue goes unmapped.
	GameplayCueName = GameplayCueTag.GetTagName();
}

void UGC_ZZZ_DamageNumber::PostInitProperties()
{
	Super::PostInitProperties();

	// The engine's asset-name derivation blanks GameplayCueName whenever the
	// derived tag is not registered ("GC_DamageNumber" → unregistered
	// "GameplayCue.DamageNumber"). Re-sync the registry mirror.
	GameplayCueName = GameplayCueTag.GetTagName();
}

void UGC_ZZZ_DamageNumber::PostLoad()
{
	Super::PostLoad();

	// The serialized GameplayCueName from a pre-fix save is None and overwrites
	// what PostInitProperties set. Re-sync after deserialization (a resave then
	// persists it — see Serialize).
	GameplayCueName = GameplayCueTag.GetTagName();
}

void UGC_ZZZ_DamageNumber::Serialize(FArchive& Ar)
{
	if (Ar.IsSaving())
	{
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
		GameplayCueName = GameplayCueTag.GetTagName();
	}
}

bool UGC_ZZZ_DamageNumber::OnExecute_Implementation(
	AActor* MyTarget,
	const FGameplayCueParameters& Parameters) const
{
	if (!MyTarget)
	{
		return false;
	}

	UWorld* World = MyTarget->GetWorld();
	if (!World)
	{
		return false;
	}

	// Single-player: route to the local player's pooled damage numbers.
	AZZZPlayerController* PC = Cast<AZZZPlayerController>(World->GetFirstPlayerController());
	if (!PC)
	{
		return false;
	}

	UZZZDamageNumberPool* Pool = PC->GetDamageNumberPool();
	if (!Pool)
	{
		return false;
	}

	const FVector WorldLoc = Parameters.Location.IsNearlyZero()
		? MyTarget->GetActorLocation()
		: FVector(Parameters.Location);

	// Semantic data only: damage value + element tag. The WBP handles all
	// presentation (colors incl. element mapping live in BP via
	// GetElementColor). Element tag is empty until Phase 4 introduces elements.
	Pool->ShowDamageNumber(WorldLoc, Parameters.RawMagnitude, FGameplayTag());

	return true;
}
