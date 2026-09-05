// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZStatusGameplayEffects.h"

#include "Attributes/ZZZAttributeSet.h"

UZZZGameplayEffect_Stagger::UZZZGameplayEffect_Stagger()
{
	// Self-expiring — the caller no longer manages a removal timer.
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.35f));
}

UZZZGameplayEffect_Dead::UZZZGameplayEffect_Dead()
{
	// No tag configuration on the CDO on purpose — see header comment.
	DurationPolicy = EGameplayEffectDurationType::Infinite;
}

UZZZGameplayEffect_Stun::UZZZGameplayEffect_Stun()
{
	// No tag configuration on the CDO on purpose — see header comment.
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	// 失衡时长 (2026-09-05): 原 Infinite 无解除路径 → Daze 满后永久眩晕、敌人永不
	// 恢复行动。改 Duration 自过期: 到期 State.Stun 随 GE 移除 (C 层 GE 管理, 同
	// _Stagger 模式), 敌人恢复; 5.0s 默认, BP 子类可覆写 (Phase 5 连携窗口细化时再按怪种调)。
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(5.0f));
}

UZZZGameplayEffect_Alive::UZZZGameplayEffect_Alive()
{
	// No tag configuration on the CDO on purpose — see header comment.
	DurationPolicy = EGameplayEffectDurationType::Infinite;
}

UZZZGameplayEffect_SwitchInvulnerable::UZZZGameplayEffect_SwitchInvulnerable()
{
	// Infinite — the switch flow removes it explicitly (outgoing: FinalizeSwitchOut,
	// incoming: 1s timer). Tag granted at apply time (see header comment).
	DurationPolicy = EGameplayEffectDurationType::Infinite;
}

UZZZGameplayEffect_HitStop::UZZZGameplayEffect_HitStop()
{
	// Duration is WORLD time — duration GEs expire via a timer on the world's
	// FTimerManager (FActiveGameplayEffect::DurationHandle →
	// CheckDurationExpired), so the actor's CustomTimeDilation does NOT
	// stretch it (2026-08-15 source verification). 0.03f = 0.03s real freeze
	// — the LOW tier default; heavier hits pick a BP child with a longer
	// duration.
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.03f));

	// Override 0.01: freezes the actor via the TimeDilation → CustomTimeDilation
	// bridge. 0.01 rather than 0.0 keeps a hair of progress for engine systems
	// that misbehave at exactly 0. On expiry the aggregator recomputes the
	// attribute — a still-running slow-motion GE resumes its value automatically.
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UZZZAttributeSet::GetTimeDilationAttribute();
	Modifier.ModifierOp = EGameplayModOp::Override;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.01f));
	Modifiers.Add(Modifier);
}

UZZZGameplayEffect_SlowMotion::UZZZGameplayEffect_SlowMotion()
{
	// Duration is WORLD time (same mechanics as _HitStop above): 1.0f = 1.0s
	// real slow-motion, not stretched by the dilation it applies.
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.0f));

	// 0.15: the enemy crawls while the player is free (player slow is the
	// lighter 0.5 GE on the player side). On expiry the aggregator recomputes
	// TimeDilation and the bridge restores the enemy to 1.0 (or whatever a
	// newer effect dictates).
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UZZZAttributeSet::GetTimeDilationAttribute();
	Modifier.ModifierOp = EGameplayModOp::Override;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.15f));
	Modifiers.Add(Modifier);
}

UZZZGameplayEffect_ParryStop::UZZZGameplayEffect_ParryStop()
{
	// Duration is WORLD time (same mechanics as _HitStop above): 0.2f = 0.2s
	// real freeze at the parry impact frame.
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.2f));

	// 0.01: pins the actor mid-pose (the montage / blend-out crawls through the
	// freeze, so the enemy stays frozen at its strike frame). On expiry the
	// aggregator recomputes TimeDilation and the bridge restores 1.0.
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UZZZAttributeSet::GetTimeDilationAttribute();
	Modifier.ModifierOp = EGameplayModOp::Override;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.01f));
	Modifiers.Add(Modifier);
}
