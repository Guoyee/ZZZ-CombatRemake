// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilityTask_WaitCombo.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "EngineUtils.h"
#include "Enemies/ZZZCombatEnemy.h"
#include "Tags/ZZZGameplayTags.h"
#include "ZZZBasicAttack.h"  // complete type: TSubclassOf<UZZZBasicAttack> conversions need StaticClass()
#include "ZZZCharacter.h"
#include "ZZZCombatRemake.h"

UZZZGameplayAbility::UZZZGameplayAbility()
{
	// Default instancing policy: InstancedPerActor so each ability activation
	// gets its own state (critical for per-hit combo tracking).
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// Default net execution: server only for single-player MVP.
	// Change to ServerInitiated for multiplayer later.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;

	// Default EndEventTag = Event.Combat.AttackEnd (2026-08-29): two-section
	// montages end their GA at the action-section notify unless a Blueprint
	// overrides (GA_Dodge → Event.Combat.DodgeEnd). The CDO is constructed
	// during module load, BEFORE native tags register (CLAUDE.md 规则 2) — a
	// bare RequestGameplayTag here would ENSURE. The tag is therefore declared
	// in Config/DefaultGameplayTags.ini (GameplayTagList) so it exists at CDO
	// time. GetEndEventTag() still falls back lazily as a safety net.
	EndEventTag = FGameplayTag::RequestGameplayTag(FName("Event.Combat.AttackEnd"));
}

FGameplayTag UZZZGameplayAbility::GetEndEventTag() const
{
	return EndEventTag.IsValid()
		? EndEventTag
		: FZZZGameplayTags::Get().Event_Combat_AttackEnd;
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
	// section (Event.Combat.AttackEnd by default — see GetEndEventTag) ends the
	// ability early — tags/blocking drop, the character is actionable again —
	// while the montage's transition section keeps playing unowned
	// (PlayMontage's bStopWhenAbilityEnds=false). Single-point event, so no
	// counting/guarding logic needed (a covered montage never fires its notify).
	const FGameplayTag EffectiveEndTag = GetEndEventTag();
	if (EffectiveEndTag.IsValid())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			FGameplayEventMulticastDelegate& EventDelegate =
				ASC->GenericGameplayEventCallbacks.FindOrAdd(EffectiveEndTag);
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
			if (FGameplayEventMulticastDelegate* Delegate = ASC->GenericGameplayEventCallbacks.Find(GetEndEventTag()))
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

void UZZZGameplayAbility::TrySetupComboHandoff()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return;
	}

	ComboHandoffTask = UAbilityTask_WaitCombo::WaitCombo(
		this,
		FZZZGameplayTags::Get().Effect_Ability_CanCombo,
		FZZZGameplayTags::Get().Input_Attack);
	ComboHandoffTask->OnComboTriggered.AddDynamic(this, &UZZZGameplayAbility::OnComboHandoffTriggered);
	ComboHandoffTask->ReadyForActivation();
}

void UZZZGameplayAbility::OnComboHandoffTriggered()
{
	// 切换退场守卫 (2026-08-31，自 UZZZBasicAttack::CheckComboTransition 迁入)：
	// 退场等待期旧角色攻击蒙太奇仍在播，其 WaitCombo 仍可能被共享 PC 缓冲触发——
	// 抑制连段与 EndAbility；GA 由 Event.Combat.AttackEnd notify 正常结束
	// （EndEventTag 路径），退场流程随之启动。
	if (AZZZCharacter* Avatar = Cast<AZZZCharacter>(GetAvatarActorFromActorInfo()))
	{
		if (Avatar->IsSwitchingOut())
		{
			return;
		}
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || !NextComboAbility)
	{
		// Terminal hit (Next null) or no ASC — stay put and keep playing; the
		// task's window-close branch has already flushed any stale buffer.
		return;
	}

	// Transition order (critical): activate the next ability FIRST, then end
	// this one — the next montage's BlendIn overlaps this ability's recovery
	// (收刀), avoiding the BlendOut→BlendIn gap frame.
	ASC->TryActivateAbilityByClass(NextComboAbility);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
