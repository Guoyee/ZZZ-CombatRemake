// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNotifyState_RotationOverride.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

void UAnimNotifyState_RotationOverride::NotifyBegin(
	USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	ACharacter* Owner = Cast<ACharacter>(MeshComp ? MeshComp->GetOwner() : nullptr);
	if (!Owner)
	{
		return;
	}

	UCharacterMovementComponent* MoveComp = Owner->GetCharacterMovement();
	if (!MoveComp)
	{
		return;
	}

	// Capture the previous state per owner before overriding (shared notify
	// instance across actors — never a single member).
	FPreviousRotation Prev;
	Prev.bOrientRotationToMovement = MoveComp->bOrientRotationToMovement;
	Prev.bUseControllerRotationYaw = Owner->bUseControllerRotationYaw;
	PreviousStates.FindOrAdd(Owner) = Prev;

	if (bDisableOrientRotationToMovement)
	{
		MoveComp->bOrientRotationToMovement = false;
	}
	if (bDisableUseControllerRotationYaw)
	{
		Owner->bUseControllerRotationYaw = false;
	}
}

void UAnimNotifyState_RotationOverride::NotifyEnd(
	USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	ACharacter* Owner = Cast<ACharacter>(MeshComp ? MeshComp->GetOwner() : nullptr);
	if (!Owner)
	{
		return;
	}

	UCharacterMovementComponent* MoveComp = Owner->GetCharacterMovement();
	if (!MoveComp)
	{
		return;
	}

	// Restore the exact previous values. Missing entry (Begin never ran for
	// this owner) → leave untouched.
	if (const FPreviousRotation* Prev = PreviousStates.Find(Owner))
	{
		MoveComp->bOrientRotationToMovement = Prev->bOrientRotationToMovement;
		Owner->bUseControllerRotationYaw = Prev->bUseControllerRotationYaw;
		PreviousStates.Remove(Owner);
	}
}

FString UAnimNotifyState_RotationOverride::GetNotifyName_Implementation() const
{
	return TEXT("Rotation Override");
}
