// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "ZZZAnimNotify_EnemyDodgeSlow.generated.h"

class UGameplayEffect;

/**
 * Perfect-dodge slow-mo payout notify (完美闪避慢动作发放点, 2026-09-03).
 *
 * Placed on the ENEMY's attack montage AFTER the damage frame (the strike has
 * been thrown — and against a perfectly-dodging player it whiffs). When it
 * fires, it checks whether this attack was perfectly dodged
 * (Effect.Enemy.Dodged — granted on the enemy by UZZZDodge when the player
 * pressed dodge inside the yellow-flash wind-up window). If so it CONSUMES
 * the flag (removes the tag) and applies SlowMotionEffect to the enemy
 * itself — the slow-mo starts here, at the 打空 moment, instead of at the
 * player's dodge press (when the enemy was still telegraphing).
 *
 * Ownership / lifecycle (layer A loose-tag, double-track):
 *   - Grant:  UZZZDodge perfect judgment (press time) → AddLooseGameplayTag.
 *   - Consume: this notify (one shot per attack — the tag is removed here).
 *   - Fallback: UZZZEnemyAttack::EndAbility removes the tag — an attack that
 *     ends before this notify (cancelled by a hit / parry / death) must not
 *     leave a stale flag that a LATER attack's notify would consume.
 * The notify must sit inside the attack's playable section (the GA is still
 * alive / montage still owned) so the EndAbility fallback cannot clear the
 * flag first. AM_EnemyAttack: damage frame at 0.42s → place this ≈0.5s.
 *
 * SlowMotionEffect defaults to the C++ carrier UZZZGameplayEffect_SlowMotion
 * (Duration 1.0s world + TimeDilation 0.15); override per notify instance
 * with a BP child / the GE_SlowMotion asset to retune.
 */
UCLASS()
class UZZZAnimNotify_EnemyDodgeSlow : public UAnimNotify
{
	GENERATED_BODY()

public:
	UZZZAnimNotify_EnemyDodgeSlow();

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	/** Slow-mo GE applied to the enemy on consume — Duration GE overriding TimeDilation. */
	UPROPERTY(EditAnywhere, Category = "Perfect Dodge Slow")
	TSubclassOf<UGameplayEffect> SlowMotionEffect;
};
