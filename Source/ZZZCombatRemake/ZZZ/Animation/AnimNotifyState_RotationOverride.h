// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_RotationOverride.generated.h"

class AActor;

/**
 * Temporarily disables CharacterMovementComponent auto-rotation so a
 * root-motion animation can drive the character's facing.
 *
 * Problem: with bOrientRotationToMovement=true (ZZZCharacter baseline), the
 * movement component forces the character toward the velocity direction every
 * frame, overriding the yaw baked into a root-motion montage — displacement
 * works but the animated rotation snaps back. This notify state turns the
 * conflicting knobs OFF during the window and restores the exact previous
 * values on end (per-owner — notify instances are shared across actors).
 *
 * Typical use: attack montages whose root motion carries a turn (挥砍转身),
 * or skills where the animator bakes the facing. Pairs with
 * AnimNotifyState_CollisionPassThrough for dash-through skills.
 *
 * ⚠ Interruption caveat (same family as CLAUDE.md 规则 1 A 层): a montage
 * interrupted mid-window may skip NotifyEnd — auto-rotation stays off until
 * the next notify pair or an ability-side restore. Keep windows short.
 */
UCLASS(meta = (DisplayName = "Rotation Override"))
class UAnimNotifyState_RotationOverride : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

	/** Turn off bOrientRotationToMovement during the window (default on — the main culprit). */
	UPROPERTY(EditAnywhere, Category = "Rotation Override")
	bool bDisableOrientRotationToMovement = true;

	/** Also turn off bUseControllerRotationYaw during the window (player default is already false). */
	UPROPERTY(EditAnywhere, Category = "Rotation Override")
	bool bDisableUseControllerRotationYaw = false;

private:
	/** Previous values captured per owner, restored on NotifyEnd. */
	struct FPreviousRotation
	{
		bool bOrientRotationToMovement = false;
		bool bUseControllerRotationYaw = false;
	};

	TMap<TWeakObjectPtr<AActor>, FPreviousRotation> PreviousStates;
};
