// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZDodge.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Enemies/ZZZCombatEnemy.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "Tags/ZZZGameplayTags.h"
#include "ZZZCombatRemake.h"
#include "Engine/World.h"

UZZZDodge::UZZZDodge()
{
	// Ability tags / activation owned tags are configured on the Blueprint
	// (GA_Dodge) — see UZZZEnemyAttack header for the CDO timing rationale.
}

void UZZZDodge::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	const float Now = GetWorld()->GetTimeSeconds();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	const FZZZGameplayTags& GameplayTags = FZZZGameplayTags::Get();

	bIsPerfectDodge = false;

	// Any new dodge clears a lingering player slow from a previous perfect
	// dodge — the displacement must always play at normal speed (the player
	// slow is applied later by the animator-placed notify, not at activation).
	if (PlayerSlowEffectHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(PlayerSlowEffectHandle);
		PlayerSlowEffectHandle.Invalidate();
	}

	// Cooldown armed by a double-dodge: reject the dodge while cooling down.
	if (Now < DodgeCooldownUntil)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Double-dodge penalty: a second dodge inside the window arms the cooldown.
	if (Now - LastDodgeTime < DoubleDodgeWindow)
	{
		DodgeCooldownUntil = Now + DodgeCooldown;
	}
	LastDodgeTime = Now;

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Dodge cancels any in-progress basic attack (any combo stage).
	if (ASC)
	{
		FGameplayTagContainer CancelTags;
		CancelTags.AddTag(GameplayTags.Ability_Attack_Basic);
		ASC->CancelAbilities(&CancelTags);
	}

	// Perfect-dodge detection (2026-08-09 design change — judgment at press
	// time): the nearest enemy within PerfectDodgeDetectRadius that carries
	// Effect.Enemy.AttackWindow (an AnimNotifyState on the ENEMY's attack
	// montage, synced with the yellow-flash wind-up warning) makes this dodge
	// "perfect". Reward: slow-motion on the enemy immediately (SlowMotionEffect,
	// 0.15 — the instant "time caught" feedback). No camera shake — the slow
	// motion itself is the reward (2026-08-16). The PLAYER slow
	// (PlayerSlowMotionEffect, 0.5) is NOT applied here — the displacement
	// plays at normal speed; it fires later from the animator-placed notify
	// (PlayerSlowEventTag). The old hit-frame CanDodge / DodgePerfect event
	// chain was removed with this change. A dodge pressed outside any window is
	// a plain dodge: i-frames + displacement, no reward.
	if (ASC && SlowMotionEffect)
	{
		if (AZZZCombatEnemy* WindowEnemy = FindNearestEnemy(
			PerfectDodgeDetectRadius, GameplayTags.Effect_Enemy_AttackWindow))
		{
			bIsPerfectDodge = true;

			UE_LOG(LogZZZCombatRemake, Log,
				TEXT("UZZZDodge: PERFECT — enemy '%s' in attack window (dist<%.0fcm)"),
				*GetNameSafe(WindowEnemy), PerfectDodgeDetectRadius);

			// Perfect-dodge status (layer E): State.PerfectDodge is granted by a
			// Duration GE (GE_PerfectDodge_Status — BP-configured TargetTags, 0.5s
			// fixed) so the follow-up attack routes to GA_DodgeCounter. Applied
			// right at activation — the counter window covers the whole slow-mo.
			if (PerfectDodgeEffect)
			{
				FGameplayEffectContextHandle PerfectContext = ASC->MakeEffectContext();
				FGameplayEffectSpecHandle PerfectSpec =
					ASC->MakeOutgoingSpec(PerfectDodgeEffect, 1.0f, PerfectContext);
				if (PerfectSpec.IsValid())
				{
					ASC->ApplyGameplayEffectSpecToSelf(*PerfectSpec.Data.Get());
				}
			}

			// Enemy slow — spec made on OUR ASC, applied on the ENEMY's ASC
			// (ApplyGameplayEffectSpecToSelf applies to the caller; the GE's
			// TargetTagRequirements on State.Enemy filters on the target side).
			if (UAbilitySystemComponent* EnemyASC = WindowEnemy->GetAbilitySystemComponent())
			{
				FGameplayEffectContextHandle EnemyContext = ASC->MakeEffectContext();
				FGameplayEffectSpecHandle EnemySpec =
					ASC->MakeOutgoingSpec(SlowMotionEffect, 1.0f, EnemyContext);
				if (EnemySpec.IsValid())
				{
					EnemyASC->ApplyGameplayEffectSpecToSelf(*EnemySpec.Data.Get());
				}
			}

		}
	}

	// Player-slow listener (2026-08-09): the montage's displacement tail fires
	// AnimNotify_SendGameplayEvent(PlayerSlowEventTag) — the animator controls
	// exactly when the player slow starts. Same pattern as the base class
	// EndEventTag listener: added here, removed in EndAbility. The notify MUST
	// sit inside the ability lifetime (before the DodgeEnd notify) or it will
	// fire into a dead ability.
	if (PlayerSlowEventTag.IsValid())
	{
		if (UAbilitySystemComponent* EventASC = GetAbilitySystemComponentFromActorInfo())
		{
			FGameplayEventMulticastDelegate& EventDelegate =
				EventASC->GenericGameplayEventCallbacks.FindOrAdd(PlayerSlowEventTag);
			PlayerSlowEventHandle = EventDelegate.AddLambda(
				[this](const FGameplayEventData*) { OnPlayerSlowStart(); });
		}
	}

	// Direction: last movement input if present, otherwise backward (-Forward).
	FVector Direction = FVector::ZeroVector;
	if (AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		if (const ACharacter* Character = Cast<ACharacter>(Avatar))
		{
			if (const UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
			{
				Direction = MoveComp->GetLastInputVector();
			}
		}
	}
	const bool bHasInput = !Direction.IsNearlyZero();
	if (!bHasInput && GetAvatarActorFromActorInfo())
	{
		const FRotator ActorRotation = GetAvatarActorFromActorInfo()->GetActorRotation();
		Direction = -FRotationMatrix(ActorRotation).GetUnitAxis(EAxis::X);
	}
	Direction = Direction.GetSafeNormal2D();

	// Direction-picked montage: forward dodge on movement input, backward
	// dodge on neutral input. Falls back to the base AttackMontage if the
	// direction-specific one is not configured.
	UAnimMontage* DodgeMontage = bHasInput ? ForwardMontage : BackMontage;
	if (!DodgeMontage)
	{
		DodgeMontage = AttackMontage;
	}

	// Procedural displacement is a fallback for placeholder montages without
	// root motion — the real dodge montages carry their own movement.
	if (bUseProceduralDisplacement)
	{
		RootMotionTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
			this, NAME_None, Direction, DodgeAcceleration, DodgeDuration,
			true /* bIsAdditive */, nullptr /* StrengthOverTime */,
			ERootMotionFinishVelocityMode::MaintainLastRootMotionVelocity,
			FVector::ZeroVector, 0.0f, false /* bEnableGravity */);
		RootMotionTask->ReadyForActivation();
	}

	// Montage + EndAbility wiring (base class template, Task 1a).
	if (!PlayMontage(DodgeMontage))
	{
		return;  // PlayMontage already ended the ability (null montage)
	}

	// Two-section dodge montage: the front section is the dodge motion, the
	// back section a stand-up → idle transition. EndEventTag (Event.Combat.DodgeEnd,
	// configured on GA_Dodge) ends the ability early — i-frames drop, all
	// actions unlock — while the montage keeps playing the transition unowned.
	// Handled by the base class; nothing to do here.
}

void UZZZDodge::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// Detach the player-slow listener so a late notify can't fire into a dead
	// ability (same discipline as the base class EndEventHandle).
	if (PlayerSlowEventHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			if (FGameplayEventMulticastDelegate* Delegate =
				ASC->GenericGameplayEventCallbacks.Find(PlayerSlowEventTag))
			{
				Delegate->Remove(PlayerSlowEventHandle);
				PlayerSlowEventHandle.Reset();
			}
		}
	}

	// Window-tag fallback (CLAUDE.md rule 1 layer A): the displacement window
	// (Effect.Ability.CanDashAttack, granted by an AbilityWindow notify state on
	// the dodge montage) must not survive the dodge — e.g. the dodge was
	// interrupted by a dash attack before NotifyEnd ran. A stale window would
	// wrongly route a LATER attack input into GA_DashAttack / GA_DodgeCounter.
	// Removal of a not-granted tag is a no-op — safe unconditionally.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->RemoveLooseGameplayTag(FZZZGameplayTags::Get().Effect_Ability_CanDashAttack);
	}

	// State.Invulnerable needs no manual removal — ActivationOwnedTags.
	// EndEventTag listener cleanup is handled by the base class.
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UZZZDodge::OnPlayerSlowStart()
{
	// The notify fires on EVERY dodge (it's baked into the montage) — only a
	// perfect dodge pays out the player slow. No event data needed: the notify
	// itself is the signal.
	if (!bIsPerfectDodge || !PlayerSlowMotionEffect)
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return;
	}

	// Player slow — the lighter decision-window slow on ourselves, applied at
	// the animator-placed moment (displacement tail). Mirrored to
	// CustomTimeDilation by the AZZZCharacter bridge.
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(PlayerSlowMotionEffect, 1.0f, Context);
	if (Spec.IsValid())
	{
		PlayerSlowEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		UE_LOG(LogZZZCombatRemake, Log,
			TEXT("UZZZDodge: player slow applied at animator-placed notify '%s'"),
			*PlayerSlowEventTag.ToString());
	}
}
