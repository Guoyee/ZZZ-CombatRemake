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
	DurationPolicy = EGameplayEffectDurationType::Infinite;
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
