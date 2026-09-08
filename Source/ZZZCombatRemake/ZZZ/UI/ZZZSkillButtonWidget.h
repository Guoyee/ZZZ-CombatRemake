// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Delegates/IDelegateInstance.h"  // FDelegateHandle (5.8: Misc/DelegateHandle.h 已并入)
#include "ZZZSkillButtonWidget.generated.h"

struct FOnAttributeChangeData;
class AZZZCharacter;

/**
 * 特殊技技能按钮（右下，纯状态指示——点击不响应，输入走既有 E 键 EnhancedInput）。
 *
 * 分工：
 *   C++ — 绑定操作角色（Possessed pawn）ASC 的 Energy 值变化 delegate，只在
 *         "能量够强化释放"这一就绪态翻转时推一次；每次 Possess 由
 *         UZZZPlayerHUDWidget 重绑（SetBoundMember）。
 *   BP (WBP_ZZZSkillButton) — 展示：就绪 = 变彩色，未就绪 = 灰暗；E 键角标
 *         （静态文本）。不做能量数值/环形指示（2026-09-08 定案）。
 *
 * 就绪阈值硬编码 50 = GA_Koleda_SpecialAttack 的 EnergyCost（UI-Design §2.3；
 * 随 UZZZCharacterData 落地后收敛为共享数据源）。不做忙态遮罩（键盘输入缓冲已
 * 处理忙时按键，忙态遮罩 = polish，见 UI-Design §七.2）。
 */
UCLASS()
class UZZZSkillButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 绑定操作角色（同角色重复调用幂等——避免每次 Possess 都重绑当前角色）。 */
	void SetBoundMember(AZZZCharacter* InMember);

	/** 当前无操作角色（未 Possess 调试态）时的解绑入口。 */
	void UnbindMember();

	// === BP 展示入口（WBP_ZZZSkillButton 实现） ===

	/**
	 * 就绪态变化时推一次：true = 能量足量、可强化释放（按钮变彩色）。
	 * 只在翻转瞬间触发（回能 1/s 不必每跳都刷）；绑定/切换角色时强制推一次，
	 * 保证切人后按钮立刻反映新成员状态。
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ZZZ|HUD")
	void BP_OnReadyChanged(bool bReady);

private:
	void OnEnergyChanged(const FOnAttributeChangeData& Data);

	TWeakObjectPtr<AZZZCharacter> BoundMember;
	FDelegateHandle EnergyChangeHandle;

	/** 就绪阈值——与 GA_Koleda_SpecialAttack EnergyCost 一致（UI-Design §2.3）。 */
	static constexpr float SpecialEnergyCost = 50.0f;

	/** 就绪态缓存——只在翻转时推 BP（切人绑定路径强制推，见 SetBoundMember）。 */
	bool bIsReady = false;
};
