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
#include "ZZZFollowUpAttack.h"  // complete type: TSubclassOf<UZZZFollowUpAttack> conversions
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
	// "perfect". Pressing dodge inside the flash means the incoming attack is
	// successfully evaded — no hit-frame confirmation is needed.
	//
	// Reward split (2026-09-03): we only FLAG the enemy here
	// (Effect.Enemy.Dodged, loose tag). The slow-mo itself is paid by a
	// consume notify (UZZZAnimNotify_EnemyDodgeSlow) on the ENEMY's attack
	// montage, placed AFTER the damage frame — the strike plays out at normal
	// speed and the slow starts once it has been thrown and missed (打空后),
	// not at the dodge press while the enemy is still telegraphing.
	// UZZZEnemyAttack::EndAbility removes the flag as the fallback (layer A
	// double-track: this dodge only flags — the notify/GA owns the tag's end).
	// No camera shake — the slow motion itself is the reward (2026-08-16). The
	// PLAYER slow (PlayerSlowMotionEffect, 0.5) is NOT applied here — the
	// displacement plays at normal speed; it fires later from the
	// animator-placed notify (PlayerSlowEventTag). A dodge pressed outside any
	// window is a plain dodge: i-frames + displacement, no reward.
	if (ASC)
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
			// right at activation — the counter window covers the slow-mo.
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

			// 被闪避标记 (2026-09-03): 挂到敌人攻击生命周期上的 A 层标志。敌人攻击
			// 蒙太奇上伤害帧之后的 EnemyDodgeSlow notify 消费它并对自己施放缓放 GE;
			// 攻击 GA EndAbility 兜底清理。只挂标记、不放慢放——慢动作起点由敌人
			// 蒙太奇的 notify 位置决定(打空后), 不再由闪避按下时决定。
			if (UAbilitySystemComponent* EnemyASC = WindowEnemy->GetAbilitySystemComponent())
			{
				EnemyASC->AddLooseGameplayTag(GameplayTags.Effect_Enemy_Dodged);
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

	// Follow-up strike handoff (2026-09-03): the dodge's displacement tail
	// carries the generic CanCombo window (same AbilityWindow notify as basic
	// segments — CanDashAttack retired); an attack input inside it hands off
	// to the dash attack / dodge counter through the base-class combo
	// machinery. Spawned unconditionally (same as UZZZBasicAttack): if the
	// dodge never reaches the window the task dies with this ability, and the
	// window-close branch doubles as the stale-buffer flush.
	TrySetupComboHandoff();

	// Two-section dodge montage: the front section is the dodge motion, the
	// back section a stand-up → idle transition. EndEventTag (Event.Combat.DodgeEnd,
	// configured on GA_Dodge) ends the ability early — i-frames drop, all
	// actions unlock — while the montage keeps playing the transition unowned.
	// Handled by the base class; nothing to do here.
}

TSubclassOf<UGameplayAbility> UZZZDodge::GetComboNext() const
{
	// 判定前移(2026-08-09)的完美闪避结果在按下时已定——窗口内攻击的追击段据此选择。
	// 空槽 = 该情形无追击: 组合交接早退, 攻击落到普攻起手(GA_01)。
	return bIsPerfectDodge ? PerfectFollowUpAbility : DashFollowUpAbility;
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
	// (Effect.Ability.CanCombo — 2026-09-03 统一后 dodge 蒙太奇挂通用 CanCombo,
	// 替代原 CanDashAttack) must not survive the dodge — e.g. the dodge was
	// interrupted by a dash attack before NotifyEnd ran. A stale window would
	// wrongly route a LATER attack input into GA_DashAttack / GA_DodgeCounter
	// (or admit a special attack out of a dead dodge). Removal of a not-granted
	// tag is a no-op — safe unconditionally.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->RemoveLooseGameplayTag(FZZZGameplayTags::Get().Effect_Ability_CanCombo);
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
