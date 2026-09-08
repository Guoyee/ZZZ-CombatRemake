// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "ZZZAnimNotify_RotateCapsuleYaw.generated.h"

/**
 * Capsule yaw snap notify (蒙太奇收尾 capsule 转向, 2026-09-07).
 *
 * Fires once and rotates the owning CHARACTER's capsule (actor root) around the
 * world-Z axis by YawDegrees — the whole actor turns instantly, capsule +
 * attached mesh together. 用途: 蒙太奇内含转身动作但动画没有驱动 capsule 的场合,
 * 在蒙太奇末尾帧摆一个, 让 capsule 朝向与动画结束姿势收敛到同一方向, 过渡到
 * 后续动画(idle / 追击段)时不回弹翻转。典型: 支援突击穿敌后收势转身 180°
 * (AM_Koleda_SwitchIn_Attack_Ex 末尾)。
 *
 * 默认 180 (向后转); 同一 notify 可配任意角度(含负值反向)。Pitch/Roll 保持
 * 不动(地面站姿下恒 ~0), 与 AbilityTask_RotateToTarget 的赋值口径一致。
 *
 * ⚠ 注意: 旋转是瞬时的一帧内整actor水平转 YawDegrees——放置点应选在动画自身
 * 转身已完成、姿势稳定处, 否则会叠加成可察觉的瞬转。打断时 notify 不触发
 * (触发型 notify 无双轨问题)。
 */
UCLASS(meta = (DisplayName = "Rotate Capsule Yaw"))
class UZZZAnimNotify_RotateCapsuleYaw : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

	/** 水平旋转量(度)。默认 180 = 向后转身; 负值反向。 */
	UPROPERTY(EditAnywhere, Category = "Rotate Capsule Yaw")
	float YawDegrees = 180.0f;
};
