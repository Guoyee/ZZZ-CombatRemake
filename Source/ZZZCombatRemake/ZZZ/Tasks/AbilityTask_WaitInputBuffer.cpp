// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilityTask_WaitInputBuffer.h"
#include "AbilitySystemComponent.h"
#include "ZZZGameplayTags.h"
#include "ZZZPlayerController.h"

UAbilityTask_WaitInputBuffer* UAbilityTask_WaitInputBuffer::WaitInputBuffer(
	UGameplayAbility* OwningAbility,
	FGameplayTag InInputTag)
{
	UAbilityTask_WaitInputBuffer* Task = NewAbilityTask<UAbilityTask_WaitInputBuffer>(OwningAbility);
	Task->InputTag = InInputTag;
	return Task;
}

void UAbilityTask_WaitInputBuffer::Activate()
{
	Super::Activate();

	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (!ASC) return;

	FGameplayEventMulticastDelegate& Delegate =
		ASC->GenericGameplayEventCallbacks.FindOrAdd(InputTag);
	InputPressedHandle = Delegate.AddUObject(
		this, &UAbilityTask_WaitInputBuffer::OnInputPressed);
}

void UAbilityTask_WaitInputBuffer::OnDestroy(bool bInOwnerFinished)
{
	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (ASC && InputPressedHandle.IsValid())
	{
		FGameplayEventMulticastDelegate* Delegate =
			ASC->GenericGameplayEventCallbacks.Find(InputTag);
		if (Delegate) { Delegate->Remove(InputPressedHandle); }
		InputPressedHandle.Reset();
	}

	Super::OnDestroy(bInOwnerFinished);
}

void UAbilityTask_WaitInputBuffer::OnInputPressed(const FGameplayEventData* Payload)
{
	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (!ASC || !ASC->HasMatchingGameplayTag(FZZZGameplayTags::Get().Effect_Input_CanBuffer))
	{
		return;
	}

	AActor* Avatar = Ability->GetAvatarActorFromActorInfo();
	if (!Avatar) return;

	AZZZPlayerController* PC = Cast<AZZZPlayerController>(Avatar->GetInstigatorController());
	if (PC)
	{
		PC->SetBufferedInput(InputTag);
		OnInputBuffered.Broadcast();
	}
}
