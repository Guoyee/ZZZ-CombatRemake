// Copyright Epic Games, Inc. All Rights Reserved.

#include "GC_ZZZ_EnemyAttackWarning.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Tags/ZZZGameplayTags.h"

UGC_ZZZ_EnemyAttackWarning::UGC_ZZZ_EnemyAttackWarning()
{
	// Single source of truth — registered in DefaultGameplayTags.ini
	// (engine-load time, before any CDO ctor) with a defensive manager lookup:
	// a CDO constructor can run before native tag registration, and a BP child
	// inherits this value — an empty tag here silently kills registration.
	GameplayCueTag = FZZZGameplayTags::Get().GameplayCue_ZZZ_EnemyAttackWarning;
	if (!GameplayCueTag.IsValid())
	{
		GameplayCueTag = FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.EnemyAttackWarning"), false);
	}

	// GameplayCueName is the AssetRegistrySearchable mirror of GameplayCueTag
	// (see GameplayCueNotify_Static.h). The CueManager's scan path
	// (BuildCuesToAddToGlobalSet) reads GameplayCueName from the FAssetData —
	// NOT the CDO — so a None here means the asset is skipped at scan time and
	// the cue tag maps to INDEX_NONE ("unmapped" in
	// GameplayCue.PrintGameplayCueNotifyMap), silently dropping every execute.
	// Keep both in sync; the Blueprint child (GC_EnemyAttackWarning) also has
	// it set explicitly so the AssetRegistry tag survives resaves.
	GameplayCueName = GameplayCueTag.GetTagName();

	// Native cue classes are only reliably picked up by the CueManager when
	// marked as overrides — otherwise the tag may resolve to nothing and
	// ExecuteGameplayCue silently no-ops. (UE 5.8 renamed the property:
	// IsOverride, not bIsOverride.)
	IsOverride = true;
}

void UGC_ZZZ_EnemyAttackWarning::PostInitProperties()
{
	Super::PostInitProperties();

	// The engine's asset-name derivation (UGameplayCueNotify_Static::
	// PostInitProperties → DeriveGameplayCueTagFromAssetName) blanks
	// GameplayCueName whenever the derived tag is not registered — which is
	// always the case here ("GC_EnemyAttackWarning" derives to the unregistered
	// "GameplayCue.EnemyAttackWarning", the derivation fails, and the tag falls
	// back to the parent's while GameplayCueName is left as None). The CueManager
	// scan (BuildCuesToAddToGlobalSet) reads GameplayCueName from the FAssetData,
	// so a None here makes the cue permanently unmapped. Re-sync it.
	GameplayCueName = GameplayCueTag.GetTagName();
}

void UGC_ZZZ_EnemyAttackWarning::PostLoad()
{
	Super::PostLoad();

	// The serialized GameplayCueName from a pre-fix save is None, and the
	// deserialized value overwrites whatever PostInitProperties set. Re-sync
	// after deserialization so the in-memory CDO always carries the tag
	// mirror (a resave then persists it — see Serialize).
	GameplayCueName = GameplayCueTag.GetTagName();
}

void UGC_ZZZ_EnemyAttackWarning::Serialize(FArchive& Ar)
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

bool UGC_ZZZ_EnemyAttackWarning::OnExecute_Implementation(
	AActor* MyTarget,
	const FGameplayCueParameters& Parameters) const
{
	// Diagnostics first — no silent exits below: every early-out logs.
	UE_LOG(LogTemp, Display,
		TEXT("GC_ZZZ_EnemyAttackWarning: OnExecute target=%s WarningSystem=%s attach=%s"),
		*GetNameSafe(MyTarget),
		WarningSystem ? *WarningSystem->GetName() : TEXT("NULL"),
		Parameters.TargetAttachComponent.IsValid()
			? *GetNameSafe(Parameters.TargetAttachComponent.Get())
			: TEXT("NONE"));

	if (!MyTarget)
	{
		UE_LOG(LogTemp, Warning, TEXT("GC_ZZZ_EnemyAttackWarning: MyTarget is null — cue fired without a target actor"));
		return false;
	}

	if (!WarningSystem)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("GC_ZZZ_EnemyAttackWarning: WarningSystem not configured on %s — set it on the Blueprint child"),
			*GetNameSafe(this));
		return false;
	}

	UWorld* World = MyTarget->GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("GC_ZZZ_EnemyAttackWarning: no World for target %s"), *GetNameSafe(MyTarget));
		return false;
	}

	// Elimination guard: skip the flash if the enemy died during the wind-up
	// (e.g. the player killed it between the raise and the hit frame).
	if (UAbilitySystemComponent* ASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(MyTarget))
	{
		if (ASC->HasMatchingGameplayTag(FZZZGameplayTags::Get().State_Dead))
		{
			UE_LOG(LogTemp, Display,
				TEXT("GC_ZZZ_EnemyAttackWarning: %s is State.Dead — flash skipped"), *GetNameSafe(MyTarget));
			return false;
		}
	}

	// Attach to the attacking hand (AttachPointName accepts both sockets and
	// bones) so the flash follows the fist — a bare mesh attachment would land
	// at the mesh root, i.e. the feet. Fall back to the mesh root, then to a
	// world-space spawn at the target.
	if (USkeletalMeshComponent* MeshComp =
		Cast<USkeletalMeshComponent>(Parameters.TargetAttachComponent))
	{
		const bool bSocketOrBoneExists =
			MeshComp->DoesSocketExist(AttachSocketName) ||
			MeshComp->GetBoneIndex(AttachSocketName) != INDEX_NONE;

		if (!bSocketOrBoneExists)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("GC_ZZZ_EnemyAttackWarning: socket/bone '%s' not found on '%s' — "
					 "falling back to mesh root. Configure AttachSocketName on the Blueprint child."),
				*AttachSocketName.ToString(), *GetNameSafe(MyTarget));
		}

		UE_LOG(LogTemp, Display,
			TEXT("GC_ZZZ_EnemyAttackWarning: attaching to '%s' on %s (%s)"),
			*AttachSocketName.ToString(), *GetNameSafe(MeshComp->GetOwner()),
			bSocketOrBoneExists ? TEXT("socket/bone found") : TEXT("NOT FOUND — mesh root fallback"));

		UNiagaraFunctionLibrary::SpawnSystemAttached(
			WarningSystem,
			MeshComp,
			bSocketOrBoneExists ? AttachSocketName : NAME_None,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget,
			/*bAutoDestroy=*/true,
			/*bAutoActivate=*/true,
			ENCPoolMethod::None);
	}
	else
	{
		// No attach component provided — world-space spawn at the target.
		const FVector SpawnLoc = Parameters.Location.IsNearlyZero()
			? MyTarget->GetActorLocation()
			: FVector(Parameters.Location);

		UE_LOG(LogTemp, Display,
			TEXT("GC_ZZZ_EnemyAttackWarning: no skeletal mesh to attach to — world-space spawn at %s"),
			*SpawnLoc.ToString());

		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World,
			WarningSystem,
			SpawnLoc,
			FRotator::ZeroRotator,
			FVector::OneVector,
			/*bAutoDestroy=*/true,
			/*bAutoActivate=*/true);
	}

	return true;
}
