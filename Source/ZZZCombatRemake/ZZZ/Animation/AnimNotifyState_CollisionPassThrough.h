// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Engine/EngineTypes.h"  // ECollisionChannel / ECollisionResponse
#include "GameplayTagContainer.h"  // FGameplayTag
#include "AnimNotifyState_CollisionPassThrough.generated.h"

class AActor;

/**
 * Temporarily switches the owner character's capsule collision response to a
 * channel (pass-through): Block → Overlap during the window, restored on end.
 *
 * Use case: root-motion skills that need to physically pass THROUGH enemies
 * (冲刺/突进类动作) without being stopped by the capsule. The ZZZ damage
 * pipeline is NOT overlap-based — ZZZAnimNotify_AttackTrace does its own
 * sphere traces at the hit frame — so damage keeps working while passing
 * through (and Generate Overlap Events stays on for any other overlap logic).
 *
 * Per-owner capture/restore: the notify instance is SHARED across actors
 * playing the same montage, so the previous response is stored per owner
 * (TMap keyed by weak owner), never as a single instance value.
 *
 * ⚠ Interruption caveat (same family as CLAUDE.md 规则 1 A 层): a montage
 * interrupted mid-window may skip NotifyEnd — the pass-through would linger.
 * Place the state so it spans the whole root-motion segment and keep the
 * segment short; add ability-side restore if a skill needs a hard guarantee.
 */
UCLASS(meta = (DisplayName = "Collision Pass-Through"))
class UAnimNotifyState_CollisionPassThrough : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

	/**
	 * Object channel to flip on the owner's capsule. With bAutoResolveOpposingFaction
	 * ON (default) this is ignored for ZZZ characters — the opposing faction's
	 * capsule channel is resolved from the owner's layer-B faction tag. Manual
	 * override for non-ZZZ owners / special setups.
	 */
	UPROPERTY(EditAnywhere, Category = "Collision Pass-Through",
		meta = (EditCondition = "!bAutoResolveOpposingFaction"))
	TEnumAsByte<ECollisionChannel> AffectedChannel = ECC_Pawn;

	/** Response used during the window (default Overlap = pass through, overlap events still fire). */
	UPROPERTY(EditAnywhere, Category = "Collision Pass-Through")
	TEnumAsByte<ECollisionResponse> PassThroughResponse = ECR_Overlap;

	/**
	 * Optional gameplay tag granted on the owner's ASC during the window
	 * (e.g. State.PassThrough — consumers like UAbilityTask_RotateToTarget
	 * skip target steering while it is present, letting the root motion drive
	 * the facing through the enemy). Empty = collision only. Layer A pattern:
	 * notify-paired LooseTag; same interruption caveat as the collision flip.
	 */
	UPROPERTY(EditAnywhere, Category = "Collision Pass-Through")
	FGameplayTag WindowTag;

	/**
	 * Auto-resolve the flipped channel from the owner's layer-B faction tag
	 * (2026-08-31): player owner → EnemyCapsule, enemy owner → PlayerCapsule.
	 * The two capsule profiles (ZZZPlayer/ZZZEnemy) both IGNORE ECC_Pawn, so
	 * the old AffectedChannel default would silently do nothing — ON keeps the
	 * notify working for every already-placed instance without touching the
	 * montage assets. OFF → use AffectedChannel as before.
	 */
	UPROPERTY(EditAnywhere, Category = "Collision Pass-Through")
	bool bAutoResolveOpposingFaction = true;

private:
	/** Resolves AffectedChannel (or the opposing-faction channel) for Owner. */
	ECollisionChannel ResolveAffectedChannel(AActor* Owner) const;

	/** Per-owner previous response, restored on NotifyEnd. */
	TMap<TWeakObjectPtr<AActor>, TEnumAsByte<ECollisionResponse>> PreviousResponses;
};
