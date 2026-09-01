// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "EngineUtils.h"
#include "Enemies/ZZZCombatEnemy.h"
#include "Tags/ZZZGameplayTags.h"
#include "ZZZCombatRemake.h"

UZZZGameplayAbility::UZZZGameplayAbility()
{
	// Default instancing policy: InstancedPerActor so each ability activation
	// gets its own state (critical for per-hit combo tracking).
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// Default net execution: server only for single-player MVP.
	// Change to ServerInitiated for multiplayer later.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
}

UAbilitySystemComponent* UZZZGameplayAbility::GetZZZASC() const
{
	return GetAbilitySystemComponentFromActorInfo();
}

AActor* UZZZGameplayAbility::GetZZZAvatarActor() const
{
	return GetAvatarActorFromActorInfo();
}

AZZZCombatEnemy* UZZZGameplayAbility::FindNearestEnemy(
	float Radius, const FGameplayTag& RequiredState) const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return nullptr;
	}

	UWorld* World = Avatar->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const FZZZGameplayTags& GameplayTags = FZZZGameplayTags::Get();
	AZZZCombatEnemy* Best = nullptr;
	float BestDistSq = Radius * Radius;

	for (TActorIterator<AZZZCombatEnemy> It(World); It; ++It)
	{
		AZZZCombatEnemy* Candidate = *It;
		if (!Candidate || Candidate->IsHidden())
		{
			continue;
		}

		UAbilitySystemComponent* CandidateASC = Candidate->GetAbilitySystemComponent();
		if (!CandidateASC
			|| CandidateASC->HasMatchingGameplayTag(GameplayTags.State_Dead))
		{
			continue;  // eliminated
		}

		if (RequiredState.IsValid()
			&& !CandidateASC->HasMatchingGameplayTag(RequiredState))
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(
			Avatar->GetActorLocation(), Candidate->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Candidate;
		}
	}

	return Best;
}

void UZZZGameplayAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Two-section montage support: an AnimNotify near the end of the action
	// section (e.g. Event.Combat.AttackEnd / Event.Combat.DodgeEnd) ends the
	// ability early — tags/blocking drop, the character is actionable again —
	// while the montage's transition section keeps playing unowned
	// (PlayMontage's bStopWhenAbilityEnds=false). Single-point event, so no
	// counting/guarding logic needed (a covered montage never fires its notify).
	if (EndEventTag.IsValid())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			FGameplayEventMulticastDelegate& EventDelegate =
				ASC->GenericGameplayEventCallbacks.FindOrAdd(EndEventTag);
			EndEventHandle = EventDelegate.AddLambda([this](const FGameplayEventData* Payload)
			{
				EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
			});
		}
	}
}

void UZZZGameplayAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// Detach the early-end listener so a late notify can't fire into a dead ability.
	if (EndEventHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			if (FGameplayEventMulticastDelegate* Delegate = ASC->GenericGameplayEventCallbacks.Find(EndEventTag))
			{
				Delegate->Remove(EndEventHandle);
				EndEventHandle.Reset();
			}
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UZZZGameplayAbility::PlayAttackMontage()
{
	return PlayMontage(AttackMontage);
}

bool UZZZGameplayAbility::PlayMontage(UAnimMontage* Montage)
{
	if (!Montage)
	{
		UE_LOG(LogZZZCombatRemake, Warning,
			TEXT("%s: montage is null — ending ability."), *GetName());
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return false;
	}

	// bStopWhenAbilityEnds=false: the montage keeps playing past EndAbility —
	// combos rely on the next ability's BlendIn overlapping this montage's
	// recovery (收刀) section, and the dodge's recovery transition plays
	// unowned after the dodge ability ends. Phase 2 verified; do not change.
	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, Montage, 1.0f, NAME_None, false);
	MontageTask->OnCompleted.AddDynamic(this, &UZZZGameplayAbility::OnMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &UZZZGameplayAbility::OnMontageBlendOut);
	MontageTask->OnInterrupted.AddDynamic(this, &UZZZGameplayAbility::OnMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UZZZGameplayAbility::OnMontageCancelled);
	MontageTask->ReadyForActivation();
	return true;
}

void UZZZGameplayAbility::OnMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UZZZGameplayAbility::OnMontageBlendOut()
{
	// Deliberate: BlendOut and Completed may both fire — the engine guards
	// double-EndAbility. Phase 2 verified; do not "fix" this pair.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UZZZGameplayAbility::OnMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UZZZGameplayAbility::OnMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
