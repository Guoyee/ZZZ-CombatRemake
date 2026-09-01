// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilityTask_WaitCombo.h"
#include "AbilitySystemComponent.h"
#include "ZZZCharacter.h"
#include "ZZZPlayerController.h"

UAbilityTask_WaitCombo* UAbilityTask_WaitCombo::WaitCombo(
	UGameplayAbility* OwningAbility,
	FGameplayTag InComboWindowTag,
	FGameplayTag InAttackInputTag)
{
	UAbilityTask_WaitCombo* Task = NewAbilityTask<UAbilityTask_WaitCombo>(OwningAbility);
	Task->ComboWindowTag = InComboWindowTag;
	Task->AttackInputTag = InAttackInputTag;
	return Task;
}

void UAbilityTask_WaitCombo::Activate()
{
	Super::Activate();

	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (!ASC) return;

	bHasTriggered = false;

	TagChangeHandle = ASC->RegisterGameplayTagEvent(ComboWindowTag, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UAbilityTask_WaitCombo::OnComboWindowChanged);
}

void UAbilityTask_WaitCombo::OnDestroy(bool bInOwnerFinished)
{
	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (ASC)
	{
		if (TagChangeHandle.IsValid())
		{
			ASC->RegisterGameplayTagEvent(ComboWindowTag, EGameplayTagEventType::NewOrRemoved)
				.Remove(TagChangeHandle);
		}
		if (AttackHandle.IsValid())
		{
			FGameplayEventMulticastDelegate* D = ASC->GenericGameplayEventCallbacks.Find(AttackInputTag);
			if (D) { D->Remove(AttackHandle); }
		}
	}

	Super::OnDestroy(bInOwnerFinished);
}

void UAbilityTask_WaitCombo::OnComboWindowChanged(FGameplayTag Tag, int32 NewCount)
{
	// 切换退场守卫 (2026-08-31)：旧角色退场期（bSwitchingOut）其 WaitCombo 仍随残段
	// 蒙太奇 tick，但 PC 共享缓冲已归新角色——此早退防止：① 关窗 flush（下方
	// NewCount<=0 分支）吞掉新角色的攻击缓冲；② 预缓冲触发旧角色连段（下方
	// HasBufferedInput 分支；CheckComboTransition 另有双保险）。旧角色的窗口 tag 由
	// StartSwitchOut 的 ClearCombatWindowTags 清除，无需在此处理。
	{
		AActor* SwitchingAvatar = Ability ? Ability->GetAvatarActorFromActorInfo() : nullptr;
		if (AZZZCharacter* SwitchingCharacter = Cast<AZZZCharacter>(SwitchingAvatar))
		{
			if (SwitchingCharacter->IsSwitchingOut())
			{
				return;
			}
		}
	}

	if (NewCount <= 0)
	{
		// CanCombo closed — unregister Attack
		UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
		if (ASC && AttackHandle.IsValid())
		{
			FGameplayEventMulticastDelegate* D = ASC->GenericGameplayEventCallbacks.Find(AttackInputTag);
			if (D) { D->Remove(AttackHandle); }
			AttackHandle.Reset();
		}

		// Flush any stale buffered input. If the window closes without the
		// buffer being consumed (e.g. the attack was interrupted/cancelled),
		// the residue would otherwise trigger an instant combo on the NEXT
		// attack's window open — skipping the intended startup hit.
		AActor* Avatar = Ability ? Ability->GetAvatarActorFromActorInfo() : nullptr;
		if (Avatar)
		{
			if (AZZZPlayerController* PC = Cast<AZZZPlayerController>(Avatar->GetInstigatorController()))
			{
				PC->ConsumeBufferedInput();
			}
		}
		return;
	}

	// CanCombo opened — check pre-buffered input
	AActor* Avatar = Ability->GetAvatarActorFromActorInfo();
	if (!Avatar) return;

	AZZZPlayerController* PC = Cast<AZZZPlayerController>(Avatar->GetInstigatorController());
	if (PC && PC->HasBufferedInput())
	{
		bHasTriggered = true;
		PC->ConsumeBufferedInput();
		OnComboTriggered.Broadcast();
		return;
	}

	// No pre-buffered input — register Attack for ongoing window
	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (!ASC) return;

	FGameplayEventMulticastDelegate& D = ASC->GenericGameplayEventCallbacks.FindOrAdd(AttackInputTag);
	AttackHandle = D.AddUObject(this, &UAbilityTask_WaitCombo::OnAttackInput);
}

void UAbilityTask_WaitCombo::OnAttackInput(const FGameplayEventData* Payload)
{
	if (bHasTriggered) return;

	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (!ASC || !ASC->HasMatchingGameplayTag(ComboWindowTag)) return;

	bHasTriggered = true;
	OnComboTriggered.Broadcast();
}
