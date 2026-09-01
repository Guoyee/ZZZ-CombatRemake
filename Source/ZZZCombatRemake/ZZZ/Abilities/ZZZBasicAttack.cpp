// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZBasicAttack.h"
#include "AbilitySystemComponent.h"
#include "ZZZGameplayTags.h"
#include "AbilityTask_WaitInputBuffer.h"
#include "AbilityTask_WaitCombo.h"
#include "AbilityTask_RotateToTarget.h"
#include "ZZZCharacter.h"

UZZZBasicAttack::UZZZBasicAttack()
{
}

void UZZZBasicAttack::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Montage playback (template lifted to the base class — Task 1a)
	if (!PlayAttackMontage())
	{
		return;  // PlayAttackMontage already ended the ability (null montage)
	}

	// Task 1: RotateToTarget (optional — auto-aim toward nearest enemy)
	if (bRotateToTarget)
	{
		RotateToTargetTask = UAbilityTask_RotateToTarget::RotateToTarget(
			this, TargetSearchRadius, RotateInterpSpeed);
		RotateToTargetTask->ReadyForActivation();
	}

	// Task 2: WaitInputBuffer
	InputBufferTask = UAbilityTask_WaitInputBuffer::WaitInputBuffer(
		this, FZZZGameplayTags::Get().Input_Attack);
	InputBufferTask->ReadyForActivation();

	// Task 3: WaitCombo — event-driven
	ComboCheckTask = UAbilityTask_WaitCombo::WaitCombo(
		this,
		FZZZGameplayTags::Get().Effect_Ability_CanCombo,
		FZZZGameplayTags::Get().Input_Attack);
	ComboCheckTask->OnComboTriggered.AddDynamic(this, &UZZZBasicAttack::OnComboTriggered);
	ComboCheckTask->ReadyForActivation();
}

void UZZZBasicAttack::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// Window-tag fallback (CLAUDE.md rule 1 layer A): a montage interruption
	// may skip AnimNotifyState::NotifyEnd, which would leave CanCombo /
	// CanBuffer stuck on the ASC and wrongly route the next attack's input.
	// Removal of a not-granted tag is a no-op — safe unconditionally.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		const FZZZGameplayTags& GameplayTags = FZZZGameplayTags::Get();
		ASC->RemoveLooseGameplayTag(GameplayTags.Effect_Ability_CanCombo);
		ASC->RemoveLooseGameplayTag(GameplayTags.Effect_Input_CanBuffer);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UZZZBasicAttack::CheckComboTransition()
{
	// 切换退场守卫 (2026-08-31)：退场等待期旧角色攻击蒙太奇仍在播，其 WaitCombo 仍
	// 可能被共享 PC 缓冲触发——抑制连段与 EndAbility；GA 由 Event.Combat.AttackEnd
	// notify 正常结束（UZZZGameplayAbility 基类 EndEventTag 路径），退场流程随之启动。
	if (AZZZCharacter* Avatar = Cast<AZZZCharacter>(GetAvatarActorFromActorInfo()))
	{
		if (Avatar->IsSwitchingOut())
		{
			return;
		}
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || !NextComboAbility) return;

	ASC->TryActivateAbilityByClass(NextComboAbility);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UZZZBasicAttack::OnComboTriggered()
{
	CheckComboTransition();
}
