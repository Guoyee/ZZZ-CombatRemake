// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZSpecialAttack.h"
#include "AbilitySystemComponent.h"
#include "AbilityTask_RotateToTarget.h"
#include "Attributes/ZZZAttributeSet.h"
#include "Effects/ZZZEnergyGameplayEffects.h"
#include "Tags/ZZZGameplayTags.h"
#include "ZZZCombatRemake.h"

const FName UZZZSpecialAttack::QuickEntrySectionName(TEXT("QuickStrike"));

UZZZSpecialAttack::UZZZSpecialAttack()
{
}

void UZZZSpecialAttack::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FZZZGameplayTags& GameplayTags = FZZZGameplayTags::Get();

	// 1) Enhanced branch lives HERE (energy is ability-owned state, not an
	// input-gate decision — the character gate never picks normal/enhanced).
	const float CurrentEnergy = ASC->GetNumericAttribute(UZZZAttributeSet::GetEnergyAttribute());
	const bool bEnhanced = CurrentEnergy >= EnergyCost;

	if (bEnhanced)
	{
		// 2) Spend. Manual apply — engine AbilityCosts clone the CDO spec and
		// cannot carry a per-instance SetByCaller magnitude. All energy changes
		// go through UZZZGameplayEffect_EnergyDelta (aggregator + value-change
		// delegate stay live); magnitude must be set BEFORE apply.
		FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
		FGameplayEffectSpecHandle CostSpec =
			ASC->MakeOutgoingSpec(UZZZGameplayEffect_EnergyDelta::StaticClass(), 1.0f, Ctx);
		if (CostSpec.IsValid())
		{
			CostSpec.Data->SetSetByCallerMagnitude(GameplayTags.Data_Energy, -EnergyCost);
			ASC->ApplyGameplayEffectSpecToSelf(*CostSpec.Data.Get());
		}
	}

	// 3) Quick entry (skip strike 1, 段 2/4 衔接): the flag rides the
	// TriggerEventData of the character gate's TryActivateAbility call — no
	// AbilityTriggers, no basic-montage notifies (see class comment).
	const bool bQuickEntry = TriggerEventData
		&& TriggerEventData->EventTag == GameplayTags.Event_Combat_SpecialQuickEntry;

	// 4) Decision-window end (FollowUp template): the perfect-dodge player
	// slow exists only to give the player time to input the follow-up. Once
	// the special activates the slow ends — otherwise it would play at 0.5x.
	// No-op on normal paths.
	{
		FGameplayTagContainer SlowTags;
		SlowTags.AddTag(GameplayTags.State_SlowMotion);
		ASC->RemoveActiveEffectsWithGrantedTags(SlowTags);
	}

	// 5) Rotate, then play — full entries start at the first section (both
	// strikes, linear); quick entries jump to the strike-2 section.
	if (bRotateToTarget)
	{
		RotateToTargetTask = UAbilityTask_RotateToTarget::RotateToTarget(
			this, TargetSearchRadius, RotateInterpSpeed);
		RotateToTargetTask->ReadyForActivation();
	}

	UAnimMontage* MontageToPlay = bEnhanced ? EnhancedMontage : AttackMontage;
	const FName StartSection = bQuickEntry ? QuickEntrySectionName : NAME_None;

	if (bEnhanced)
	{
		UE_LOG(LogZZZCombatRemake, Log,
			TEXT("%s: ENHANCED special (energy %.0f -> %.0f), quick=%s"),
			*GetName(), CurrentEnergy, CurrentEnergy - EnergyCost,
			bQuickEntry ? TEXT("yes") : TEXT("no"));
	}
	else
	{
		UE_LOG(LogZZZCombatRemake, Log,
			TEXT("%s: normal special (energy %.0f < cost %.0f), quick=%s"),
			*GetName(), CurrentEnergy, EnergyCost,
			bQuickEntry ? TEXT("yes") : TEXT("no"));
	}

	if (!PlayMontage(MontageToPlay, StartSection))
	{
		return;  // PlayMontage already ended the ability (null montage)
	}
}

void UZZZSpecialAttack::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// Window-tag fallback (CLAUDE.md rule 1 layer A): a montage interruption
	// may skip AnimNotifyState::NotifyEnd, leaving combo-window tags stuck on
	// the ASC and wrongly routing later inputs. This ability grants no windows
	// itself — the cleanup is defensive against a mis-copied BasicAttack
	// notify layout. Removal of a not-granted tag is a no-op — safe.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		const FZZZGameplayTags& GameplayTags = FZZZGameplayTags::Get();
		ASC->RemoveLooseGameplayTag(GameplayTags.Effect_Ability_CanCombo);
		ASC->RemoveLooseGameplayTag(GameplayTags.Effect_Input_CanBuffer);
		ASC->RemoveLooseGameplayTag(GameplayTags.State_Combat_Recovery);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
