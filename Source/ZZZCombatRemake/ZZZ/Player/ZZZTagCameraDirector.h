// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/CameraDirector.h"
#include "GameplayTagContainer.h"
#include "ZZZTagCameraDirector.generated.h"

class UCameraRigAsset;

/**
 * Tag → rig 映射（多角色多技能镜头管理，2026-09-11）。
 *
 * Tag 配在 GA 的 ActivationOwnedTags 上（随能力生命周期自动挂/清，零手写生命周期）；
 * director 每帧查角色 ASC 的 owned tags，取最高优先级命中项的 rig。
 * 新角色/新技能的镜头 = GA 加一个 tag + 本表加一行，无需 C++、无需改 rig 资产。
 */
USTRUCT(BlueprintType)
struct FZZZCameraTagMapping
{
	GENERATED_BODY()

	/** 触发 tag（如 Camera.Closeup.Parry）。 */
	UPROPERTY(EditAnywhere, Category = "ZZZ|Camera")
	FGameplayTag Tag;

	/** 命中时激活的 rig（须属于本 CameraAsset —— 即 /Game/ZZZ/Camera/ 下的 rig）。 */
	UPROPERTY(EditAnywhere, Category = "ZZZ|Camera", meta = (UseSelfCameraRigPicker = true))
	TObjectPtr<UCameraRigAsset> CameraRig;

	/** 多个 tag 同时命中时取最高优先级（同优先级取列表靠前者）。 */
	UPROPERTY(EditAnywhere, Category = "ZZZ|Camera")
	int32 Priority = 0;
};

/**
 * Tag 驱动的相机 director（2026-09-11）—— 替代原 CDE_PlayerCamera(BP)。
 *
 * 每帧（FZZZTagCameraDirectorEvaluator::OnRun）：
 *   owner（角色的 GameplayCamera 组件）→ GetOwner() → Pawn → ASC → OwnedGameplayTags
 *   → 遍历 TagMappings 取最高优先级命中项 → OutResult.Add(该 rig)
 *   无命中 → DefaultRig（主 rig CR_ThirdPerson）
 *
 * 这是 rig 的唯一激活点：director 每帧 push 的请求会顶掉任何绕路 push
 * （TransientBlendStackCameraNode::Push 去重只对"栈顶同 context 同 rig"生效）；
 * "归还"= 本 director 下一帧改 push DefaultRig —— 引擎无"rig 自动归还"
 * （ExitTransitions 只是 blend 资产，详见 Docs/ZZZ-Camera-Architecture.md §四）。
 *
 * 配置（CA_PlayerCameras 细节面板）：
 *   - Camera Director → 选本类（替换原 CDE_PlayerCamera BP）
 *   - Default Rig = CR_ThirdPerson
 *   - Tag Mappings = 每技能一行：Tag + Rig + Priority
 * 技能侧：GA 的 ActivationOwnedTags 加对应 tag 即生效（tag 存活期 = 能力存活期
 * ——特写持续到 GA 结束，支援突击接管时自动归还）。
 */
UCLASS(EditInlineNew, Blueprintable)
class UZZZTagCameraDirector : public UCameraDirector
{
	GENERATED_BODY()

public:
	UZZZTagCameraDirector(const FObjectInitializer& ObjectInit);

	/** 无 tag 命中时的兜底 rig（主 rig，如 CR_ThirdPerson）。 */
	UPROPERTY(EditAnywhere, Category = "ZZZ|Camera", meta = (UseSelfCameraRigPicker = true))
	TObjectPtr<UCameraRigAsset> DefaultRig;

	/** Tag → Rig 映射表（Priority 高者胜）。 */
	UPROPERTY(EditAnywhere, Category = "ZZZ|Camera")
	TArray<FZZZCameraTagMapping> TagMappings;

protected:
	// UCameraDirector interface.
	virtual FCameraDirectorEvaluatorPtr OnBuildEvaluator(FCameraDirectorEvaluatorBuilder& Builder) const override;
	virtual void OnGatherRigUsageInfo(FCameraDirectorRigUsageInfo& UsageInfo) const override;
};
