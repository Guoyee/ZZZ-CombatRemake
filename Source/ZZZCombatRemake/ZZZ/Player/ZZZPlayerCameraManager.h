// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameplayCamerasPlayerCameraManager.h"
#include "ZZZPlayerCameraManager.generated.h"

/**
 * PlayerCameraManager for ZZZ combat (GameplayCameras manager mode 子类).
 *
 * 唯一职责（2026-09-04）：玩家相机俯仰限幅 ViewPitchMin / ViewPitchMax。
 *
 * 5.8 生效点（引擎源码核实，勿按旧管线认知理解）：ViewPitchMin/Max 的唯一按帧
 * 消费点是 APlayerController::UpdateRotation 每帧对 ControlRotation 调
 * ProcessViewRotation → LimitViewPitch；GameplayCameras manager 的 DoUpdateCamera
 * 刻意不调 Super（rig 输出 GetEvaluatedCameraView 直接 FillCameraCache），旧
 * UpdateViewTarget 的"最终相机 POV 限幅"整条已不存在 → 限的是 ControlRotation。
 * 本项目主 rig（CR_ThirdPerson）旋转恒 = ControlRotation（BoomArm 无输入槽 → 引擎
 * 自动接 UDrivenControlRotationCameraNode，其写回前同样读 manager 的 ViewPitch*
 * 限值），故钳 ControlRotation = 钳最终相机俯仰（相机=瞄准共用该包络，预期行为）。
 * 边界：rig 内自累积旋转节点（orbit 自加输入）与跨限位长 blend 不经任何限幅；
 * VR 头显（IsHeadTrackingAllowed）时引擎跳过限幅。
 */
UCLASS()
class AZZZPlayerCameraManager : public AGameplayCamerasPlayerCameraManager
{
	GENERATED_BODY()

public:
	AZZZPlayerCameraManager(const FObjectInitializer& ObjectInit);
};
