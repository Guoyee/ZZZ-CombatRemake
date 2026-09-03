// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ZZZStatusGameplayEffects.generated.h"

/**
 * Bare status GameplayEffects used as tag-lifecycle carriers for CORE combat
 * states (stagger / dead / stun / alive).
 *
 * The tag itself is added at APPLY time via FGameplayEffectSpec::DynamicGrantedTags
 * — never configured on the CDO. Rationale (CLAUDE.md 实施经验 规则 1 分层 B/C):
 *   1. Core-state tags (layers B/C) must NOT use AddLooseGameplayTag — lifecycle
 *      is GE-managed (removed on effect removal / ASC reset, replicates, visible
 *      in showdebug). Same pattern as UZZZGameplayEffect_Faction.
 *   2. The CDO is constructed during module load (before StartupModule), so
 *      FZZZGameplayTags::Get() would still hold invalid tags there and a
 *      CDO-configured granted tag would be silently dropped.
 *
 * These are C++ carriers (not BP assets) because their call sites are C++
 * hardcoded logic paths with no configuration host — the AttributeSet's
 * PostGameplayEffectExecute and the enemy's BeginPlay. A TSubclassOf property
 * would push per-BP configuration onto every character/enemy Blueprint (missing
 * config = silent feature loss), and hard asset paths would break on rename.
 * Layer-E per-ability effects (e.g. GE_PerfectDodge_Status) ARE BP assets —
 * they have a natural config host (the ability's TSubclassOf property).
 *
 * Type → intended use:
 *   _Stagger  (Duration 0.35s): hit-stagger — self-expiring, no timer needed.
 *   _Dead / _Stun / _Alive (Infinite): persistent life-cycle markers.
 *   _HitStop  (Duration 0.03s): hit-stop freeze — TimeDilation override, the
 *     TimeDilation bridge mirrors it to CustomTimeDilation (see class comment).
 *   _SlowMotion (Duration 1.0s): perfect-dodge enemy slow — TimeDilation
 *     override 0.15, applied by the enemy's own dodge-slow consume notify
 *     (2026-09-03). Same Duration-world-time mechanics as _HitStop.
 */
UCLASS()
class UZZZGameplayEffect_Stagger : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UZZZGameplayEffect_Stagger();
};

UCLASS()
class UZZZGameplayEffect_Dead : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UZZZGameplayEffect_Dead();
};

UCLASS()
class UZZZGameplayEffect_Stun : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UZZZGameplayEffect_Stun();
};

UCLASS()
class UZZZGameplayEffect_Alive : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UZZZGameplayEffect_Alive();
};

/**
 * Switch-flow invulnerability (2026-08-31) — NOT a tag carrier in the usual
 * sense: the switch flow applies it with DynamicGrantedTags=State.Invulnerable
 * (layer C-style GE-managed lifecycle, but owned by the character's switch
 * state machine, not by an ability — see ZZZCharacter::ApplySwitchInvulnerability).
 *
 * Infinite + explicit removal on FinalizeSwitchOut (outgoing member) or a
 * 1.0s self-timer (incoming member, BeginSwitchIn) — the outgoing side cannot
 * use a fixed Duration because StartSwitchOut may wait arbitrarily long for an
 * attack GA to end before the exit sequence even starts; the incoming side can,
 * because the enter montage is play-and-forget within a bounded window.
 *
 * Why a handle-based GE instead of ActivationOwnedTags: the switch flow is not
 * a GameplayAbility. State.Invulnerable is shared with perfect-dodge (GE
 * BP asset / GA tags) — removal must target ONLY this effect, hence
 * FActiveGameplayEffectHandle, never RemoveActiveEffectsWithGrantedTags.
 *
 * Separate class from the perfect-dodge carrier so the switch flow never
 * touches the dodge GE (and vice versa).
 */
UCLASS()
class UZZZGameplayEffect_SwitchInvulnerable : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UZZZGameplayEffect_SwitchInvulnerable();
};

/**
 * Hit-stop freeze (卡肉) — NOT a tag carrier: a Duration GE that overrides
 * TimeDilation to 0.01 so the TimeDilation bridge freezes the actor's
 * CustomTimeDilation. Duration counts WORLD time (duration GEs expire via a
 * world FTimerManager timer — actor CustomTimeDilation does not stretch GE
 * timing, 2026-08-15 source verification), so 0.03f = 0.03s real freeze, no
 * compensation. On expiry the aggregator recomputes TimeDilation and the
 * bridge restores the actor (to a still-running slow-motion value, or 1.0).
 *
 * Default choice of ZZZAnimNotify_AttackTrace::HitStopEffect — per-notify
 * overridable with a BP child for heavier/lighter hits.
 */
UCLASS()
class UZZZGameplayEffect_HitStop : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UZZZGameplayEffect_HitStop();
};

/**
 * Perfect-dodge enemy slow (完美闪避敌人慢放) — NOT a tag carrier: a Duration GE
 * overriding TimeDilation to 0.15 so the bridge slows the enemy's
 * CustomTimeDilation. Same world-time duration semantics as _HitStop — 1.0s
 * real slow regardless of the dilation itself.
 *
 * Default choice of UZZZAnimNotify_EnemyDodgeSlow::SlowMotionEffect —
 * per-notify overridable with a BP child (or the pre-existing GE_SlowMotion
 * asset) for tuning. Applied by the ENEMY to itself when its consume notify
 * fires after a perfectly-dodged attack — the slow starts once the strike has
 * been thrown and missed (打空后), not at the player's dodge press
 * (2026-09-03).
 */
UCLASS()
class UZZZGameplayEffect_SlowMotion : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UZZZGameplayEffect_SlowMotion();
};
