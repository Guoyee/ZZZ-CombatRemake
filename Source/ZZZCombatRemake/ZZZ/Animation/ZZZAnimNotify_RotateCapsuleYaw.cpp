// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZAnimNotify_RotateCapsuleYaw.h"
#include "GameFramework/Character.h"

void UZZZAnimNotify_RotateCapsuleYaw::Notify(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	ACharacter* Owner = Cast<ACharacter>(MeshComp ? MeshComp->GetOwner() : nullptr);
	if (!Owner)
	{
		return;
	}

	// 水平(绕世界 Z)转 YawDegrees。Pitch/Roll 保持不动——胶囊根组件通常贴地,
	// 且与 AbilityTask_RotateToTarget 的口径一致, 不干扰其它俯仰系统。
	const FRotator CurrentRotation = Owner->GetActorRotation();
	Owner->SetActorRotation(
		FRotator(CurrentRotation.Pitch, CurrentRotation.Yaw + YawDegrees, CurrentRotation.Roll));
}

FString UZZZAnimNotify_RotateCapsuleYaw::GetNotifyName_Implementation() const
{
	return TEXT("Rotate Capsule Yaw");
}
