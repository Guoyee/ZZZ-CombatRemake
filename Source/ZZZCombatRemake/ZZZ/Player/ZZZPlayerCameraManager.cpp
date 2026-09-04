// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZPlayerCameraManager.h"

AZZZPlayerCameraManager::AZZZPlayerCameraManager(const FObjectInitializer& ObjectInit)
	: Super(ObjectInit)
{
	// 俯仰包络（度）：向下看 -60° / 向上看 +30°。钳的是 ControlRotation ——
	// CR_ThirdPerson 朝向以其为唯一源（见类注释），相机/瞄准共用此包络。
	ViewPitchMin = -60.0f;
	ViewPitchMax = 30.0f;
}
