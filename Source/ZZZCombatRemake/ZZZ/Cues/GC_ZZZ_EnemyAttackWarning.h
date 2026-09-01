// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "GC_ZZZ_EnemyAttackWarning.generated.h"

class UNiagaraSystem;

/**
 * GameplayCue that flashes a yellow warning on an enemy during attack wind-up.
 *
 * Triggered from the built-in engine AnimNotify "GameplayCue (Burst)"
 * (UAnimNotify_GameplayCue) placed on AM_EnemyAttack at the wind-up frames,
 * configured with the tag GameplayCue.ZZZ.EnemyAttackWarning. The notify
 * resolves the owner's ASC (Pawn-ASC) and sets TargetAttachComponent to the
 * mesh, so the flash follows the enemy body for free.
 *
 * One-shot / stateless by design (design doc 8.11, Path A): the Niagara system
 * is self-terminating, so there is no "restore" step and multiple enemies can
 * flash concurrently without state collisions. The blue "unblockable" variant
 * later is a second GCN (or a color parameter) — see design doc 8.11.
 *
 * Elimination guard: if the enemy is already State.Dead when the cue fires
 * (killed during the wind-up), the flash is skipped.
 *
 * Configure on the Blueprint child (GC_ZZZ_EnemyAttackWarning):
 *   - GameplayCueTag = GameplayCue.ZZZ.EnemyAttackWarning  (set by the native
 *     ctor from the ini-registered tag — keep the inherited value)
 *   - WarningSystem  = yellow burst Niagara asset
 */
UCLASS()
class UGC_ZZZ_EnemyAttackWarning : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	UGC_ZZZ_EnemyAttackWarning();

	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;

	virtual bool OnExecute_Implementation(
		AActor* MyTarget,
		const FGameplayCueParameters& Parameters) const override;

	/** Yellow warning burst. Self-terminating — no restore logic needed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Cue")
	TObjectPtr<UNiagaraSystem> WarningSystem;

	/**
	 * Socket (or bone) on the enemy mesh the flash attaches to — the attacking
	 * hand by default. AttachPointName accepts both sockets and bones; falls
	 * back to the mesh root when neither exists. Override per enemy type via
	 * the Blueprint child.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Cue")
	FName AttachSocketName = TEXT("hand_r");
};
