// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNotifyState_AbilityWindow.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"

void UAnimNotifyState_AbilityWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	if (!MeshComp || !WindowTag.IsValid())
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		return;
	}

	IAbilitySystemInterface* ASCInterface = Cast<IAbilitySystemInterface>(Owner);
	if (!ASCInterface)
	{
		return;
	}

	UAbilitySystemComponent* ASC = ASCInterface->GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	ASC->AddLooseGameplayTag(WindowTag);
}

void UAnimNotifyState_AbilityWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (!MeshComp || !WindowTag.IsValid())
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		return;
	}

	IAbilitySystemInterface* ASCInterface = Cast<IAbilitySystemInterface>(Owner);
	if (!ASCInterface)
	{
		return;
	}

	UAbilitySystemComponent* ASC = ASCInterface->GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	ASC->RemoveLooseGameplayTag(WindowTag);
}

FString UAnimNotifyState_AbilityWindow::GetNotifyName_Implementation() const
{
	if (WindowTag.IsValid())
	{
		return FString::Printf(TEXT("AbilityWindow: %s"), *WindowTag.ToString());
	}
	return TEXT("Ability Window");
}
