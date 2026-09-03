// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"  // FGameplayTag (CameraShakeCueTag)
#include "ZZZAnimNotify_AttackTrace.generated.h"

class UGameplayEffect;

/**
 * Custom AnimNotify for ZZZ attack hit detection.
 *
 * Does NOT depend on ICombatAttacker — directly uses IAbilitySystemInterface
 * to find the attacker's ASC and apply a GameplayEffect to hit targets.
 *
 * Each montage can host multiple instances of this notify with different
 * BaseDamage and DazeMultiplier values (e.g. hit 1 = 25 base, hit 4 = 50 base).
 * Damage values are passed to the GE via SetByCaller tags (Data.Damage,
 * Data.DazeMultiplier) and consumed by UZZZDamageExecution (ExecCalc).
 *
 * Hit feedback (打击感, Phase 3 Task 5a) is configured per instance too:
 * bApplyHitStop (default GE) + bApplyCameraShake with a shake-tier TAG
 * (Low/Mid/High, default Low) — whiffed swings never trigger feedback (the
 * trace gates the whole loop), and invulnerable / dead targets are skipped
 * via a pre-hit tag snapshot.
 */
UCLASS()
class UZZZAnimNotify_AttackTrace : public UAnimNotify
{
	GENERATED_BODY()

public:
	UZZZAnimNotify_AttackTrace();

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	/** Bone/socket to start the trace from */
	UPROPERTY(EditAnywhere, Category = "Attack Trace")
	FName DamageSourceBone = FName("hand_r");

	/** Forward trace distance (cm) */
	UPROPERTY(EditAnywhere, Category = "Attack Trace")
	float TraceDistance = 75.0f;

	/** Sphere trace radius (cm) */
	UPROPERTY(EditAnywhere, Category = "Attack Trace")
	float TraceRadius = 50.0f;

	/**
	 * GameplayEffect to apply on hit (must use UZZZDamageExecution as its
	 * ExecCalc). Defaults to the shared GE_Damage BP (/Game/ZZZ/GE/GE_Damage,
	 * loaded via FClassFinder in the CDO ctor) — override per instance for
	 * hits with special modifier setups.
	 */
	UPROPERTY(EditAnywhere, Category = "Attack Trace")
	TSubclassOf<UGameplayEffect> DamageEffect;

	/** Base damage dealt by this hit — before Defense reduction in ExecCalc. */
	UPROPERTY(EditAnywhere, Category = "Attack Trace|Damage")
	float BaseDamage = 25.0f;

	/** Absolute Daze/Stun buildup dealt by this hit (independent of damage). */
	UPROPERTY(EditAnywhere, Category = "Attack Trace|Damage")
	float DazeBuildup = 20.0f;

	// === Hit feedback (打击感, Phase 3 Task 5a) — per-instance switches ===

	/** Freeze attacker AND target briefly at the hit frame (卡肉). */
	UPROPERTY(EditAnywhere, Category = "Hit Feedback")
	bool bApplyHitStop = true;

	/**
	 * Hit-stop GE: a Duration GE overriding TimeDilation to ~0.01 (the bridge
	 * mirrors it to CustomTimeDilation). Duration counts world time — no
	 * compensation needed. Default: UZZZGameplayEffect_HitStop (0.06s freeze).
	 * Override with a BP child for heavier/lighter hits.
	 */
	UPROPERTY(EditAnywhere, Category = "Hit Feedback", meta = (EditCondition = "bApplyHitStop"))
	TSubclassOf<UGameplayEffect> HitStopEffect;

	/** Camera shake at the hit frame. */
	UPROPERTY(EditAnywhere, Category = "Hit Feedback")
	bool bApplyCameraShake = true;

	/**
	 * Shake tier executed at the hit frame — pick one of the registered
	 * GameplayCue.ZZZ.CameraShake.Low/.Mid/.High tags; the CueManager resolves
	 * the tag to the matching GC_ZZZ_CameraShake Blueprint handler (each tier
	 * = one handler with its own ShakeScale, all using the single BP_HitShake
	 * shake asset). Empty falls back to Low at runtime. Per-notify instance:
	 * light hits = Low, finishers = High.
	 */
	UPROPERTY(EditAnywhere, Category = "Hit Feedback",
		meta = (Categories = "GameplayCue", EditCondition = "bApplyCameraShake"))
	FGameplayTag CameraShakeCueTag;
};
