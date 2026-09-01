// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_RotateToTarget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRotateToTargetDelegate);

/**
 * Rotates the owning character toward the nearest enemy during an ability.
 *
 * TickTask interpolates the actor's yaw toward the target actor each frame.
 * Target is found once on Activate via sphere sweep for IAbilitySystemInterface actors.
 * Task ends automatically when the owning ability ends.
 *
 * Usage in UZZZBasicAttack::ActivateAbility:
 *   RotateToTargetTask = UAbilityTask_RotateToTarget::RotateToTarget(this, 500, 10);
 *   RotateToTargetTask->ReadyForActivation();
 */
UCLASS()
class UAbilityTask_RotateToTarget : public UAbilityTask
{
	GENERATED_BODY()

public:
	/** Broadcast when a target is successfully found. */
	UPROPERTY(BlueprintAssignable)
	FRotateToTargetDelegate OnTargetFound;

	/** Broadcast when no valid target exists within search radius. */
	UPROPERTY(BlueprintAssignable)
	FRotateToTargetDelegate OnNoTargetFound;

	/**
	 * Create the task.
	 *
	 * @param OwningAbility  The ability that owns this task.
	 * @param InSearchRadius  Sphere radius (cm) for target search.
	 * @param InInterpSpeed   Yaw interpolation speed (higher = snappier, 5-15 typical).
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks",
		meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_RotateToTarget* RotateToTarget(
		UGameplayAbility* OwningAbility,
		float InSearchRadius = 500.0f,
		float InInterpSpeed = 10.0f);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;
	virtual void OnDestroy(bool bInOwnerFinished) override;

protected:
	/** Sweep for the nearest IAbilitySystemInterface actor (excluding self). */
	AActor* FindNearestTarget() const;

	float SearchRadius;
	float InterpSpeed;

	/** The target we're rotating toward. May become invalid if target dies. */
	UPROPERTY()
	TObjectPtr<AActor> TargetActor;

	bool bTargetFound = false;
};
