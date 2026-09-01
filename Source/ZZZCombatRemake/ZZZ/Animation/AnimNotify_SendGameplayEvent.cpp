// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNotify_SendGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"

void UAnimNotify_SendGameplayEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (!MeshComp || !EventTag.IsValid())
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
		UE_LOG(LogTemp, Warning,
			TEXT("SendGameplayEvent [%s]: Owner '%s' does not implement IAbilitySystemInterface. "
			     "Place this notify on an actor with an ASC."),
			*EventTag.ToString(), *Owner->GetName());
		return;
	}

	UAbilitySystemComponent* ASC = ASCInterface->GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	FGameplayEventData EventData;
	EventData.Instigator = Owner;
	EventData.OptionalObject = OptionalPayload.Get();

	ASC->HandleGameplayEvent(EventTag, &EventData);
}

FString UAnimNotify_SendGameplayEvent::GetNotifyName_Implementation() const
{
	if (EventTag.IsValid())
	{
		return FString::Printf(TEXT("SendEvent: %s"), *EventTag.ToString());
	}
	return TEXT("Send Gameplay Event");
}
