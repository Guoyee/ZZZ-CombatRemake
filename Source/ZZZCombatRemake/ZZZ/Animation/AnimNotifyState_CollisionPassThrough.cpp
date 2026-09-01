// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNotifyState_CollisionPassThrough.h"

#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Tags/ZZZGameplayTags.h"  // State.Player / State.Enemy (faction tag → channel)

void UAnimNotifyState_CollisionPassThrough::NotifyBegin(
	USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	ACharacter* Owner = Cast<ACharacter>(MeshComp ? MeshComp->GetOwner() : nullptr);
	if (!Owner)
	{
		return;
	}

	UCapsuleComponent* Capsule = Owner->GetCapsuleComponent();
	if (!Capsule)
	{
		return;
	}

	// Capture the previous response per owner before flipping — the notify
	// instance is shared across actors, a single member would corrupt state.
	// Channel resolved per owner (auto = opposing faction's capsule channel),
	// NOT stored — deterministic re-resolution in NotifyEnd restores it.
	const ECollisionChannel ChannelToFlip = ResolveAffectedChannel(Owner);
	PreviousResponses.FindOrAdd(Owner) =
		Capsule->GetCollisionResponseToChannel(ChannelToFlip);

	Capsule->SetCollisionResponseToChannel(ChannelToFlip, PassThroughResponse);

	// Keep overlap events on so any overlap-based logic (and future hit
	// feedback) still fires while passing through.
	Capsule->SetGenerateOverlapEvents(true);

	// Optional window tag (layer A, notify-paired LooseTag): consumers query
	// it to disable steering while passing through — UAbilityTask_RotateToTarget
	// skips rotation when State.PassThrough is present. GetComponentByClass
	// covers both player (ASC) and enemy (AbilitySystemComponent) namings.
	if (WindowTag.IsValid())
	{
		if (UAbilitySystemComponent* ASC = Owner->GetComponentByClass<UAbilitySystemComponent>())
		{
			ASC->AddLooseGameplayTag(WindowTag);
		}
	}
}

void UAnimNotifyState_CollisionPassThrough::NotifyEnd(
	USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	ACharacter* Owner = Cast<ACharacter>(MeshComp ? MeshComp->GetOwner() : nullptr);
	if (!Owner)
	{
		return;
	}

	UCapsuleComponent* Capsule = Owner->GetCapsuleComponent();
	if (!Capsule)
	{
		return;
	}

	// Restore the exact previous response. Missing entry (Begin never ran for
	// this owner) → leave untouched — restoring a default could clobber a
	// legitimately different configuration. Same deterministic channel
	// resolution as NotifyBegin.
	const ECollisionChannel ChannelToFlip = ResolveAffectedChannel(Owner);
	if (const TEnumAsByte<ECollisionResponse>* Previous = PreviousResponses.Find(Owner))
	{
		Capsule->SetCollisionResponseToChannel(ChannelToFlip, *Previous);
		PreviousResponses.Remove(Owner);
	}

	// Drop the window tag with the collision flip — both belong to the same
	// window (interruption skips NotifyEnd → tag + collision linger together,
	// which is consistent). Removal of a not-granted tag is a no-op.
	if (WindowTag.IsValid())
	{
		if (UAbilitySystemComponent* ASC = Owner->GetComponentByClass<UAbilitySystemComponent>())
		{
			ASC->RemoveLooseGameplayTag(WindowTag);
		}
	}
}

ECollisionChannel UAnimNotifyState_CollisionPassThrough::ResolveAffectedChannel(
	AActor* Owner) const
{
	if (!bAutoResolveOpposingFaction)
	{
		return AffectedChannel.GetValue();
	}

	// Faction tag → opposing faction's capsule channel (2026-08-31): pass-
	// through must flip the channel the capsule BLOCKS, which is now the other
	// side's dedicated channel, not ECC_Pawn. Layer-B tags (State.Player /
	// State.Enemy) are granted by Infinite GE at InitAbilitySystem — always
	// present during gameplay windows.
	if (UAbilitySystemComponent* ASC = Owner->GetComponentByClass<UAbilitySystemComponent>())
	{
		const FZZZGameplayTags& Tags = FZZZGameplayTags::Get();
		if (ASC->HasMatchingGameplayTag(Tags.State_Player))
		{
			return ECC_GameTraceChannel2; // EnemyCapsule
		}
		if (ASC->HasMatchingGameplayTag(Tags.State_Enemy))
		{
			return ECC_GameTraceChannel1; // PlayerCapsule
		}
	}
	return AffectedChannel.GetValue();
}

FString UAnimNotifyState_CollisionPassThrough::GetNotifyName_Implementation() const
{
	return TEXT("Collision Pass-Through");
}
