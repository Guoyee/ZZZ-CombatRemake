// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZAnimNotify_EnemyDodgeSlow.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Effects/ZZZStatusGameplayEffects.h"
#include "GameplayEffect.h"
#include "Tags/ZZZGameplayTags.h"
#include "ZZZCombatRemake.h"  // LogZZZCombatRemake

UZZZAnimNotify_EnemyDodgeSlow::UZZZAnimNotify_EnemyDodgeSlow()
{
	// Default reward GE — C++ carrier (Duration 1.0s world + TimeDilation
	// 0.15), same default-in-ctor pattern as HitStopEffect on the attack
	// trace notify. Per-instance overridable in the montage editor.
	SlowMotionEffect = UZZZGameplayEffect_SlowMotion::StaticClass();
}

void UZZZAnimNotify_EnemyDodgeSlow::Notify(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (!MeshComp || !SlowMotionEffect)
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		return;
	}

	// The notify lives on the ENEMY's montage — its own ASC is the consumer
	// (also the slow target).
	IAbilitySystemInterface* GAS = Cast<IAbilitySystemInterface>(Owner);
	UAbilitySystemComponent* ASC = GAS ? GAS->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return;
	}

	const FZZZGameplayTags& Tags = FZZZGameplayTags::Get();

	// Not dodged → the flag is absent → nothing to pay (a normal swing that
	// the player simply avoided, or an attack nobody dodged).
	if (!ASC->HasMatchingGameplayTag(Tags.Effect_Enemy_Dodged))
	{
		return;
	}

	// Consume: one payout per dodged attack. If the flag somehow survived to
	// a LATER attack's notify, the tag was already removed here — no double pay.
	ASC->RemoveLooseGameplayTag(Tags.Effect_Enemy_Dodged);

	// Pay the slow on ourselves — Duration GE; the TimeDilation →
	// CustomTimeDilation bridge (AZZZCombatEnemy) mirrors it. Spec made and
	// applied on the same ASC (no cross-Actor routing needed).
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(SlowMotionEffect, 1.0f, Context);
	if (Spec.IsValid())
	{
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		UE_LOG(LogZZZCombatRemake, Log,
			TEXT("[%s] EnemyDodgeSlow: dodged attack consumed — slow-mo starts (打空后)"),
			*GetNameSafe(Owner));
	}
}
