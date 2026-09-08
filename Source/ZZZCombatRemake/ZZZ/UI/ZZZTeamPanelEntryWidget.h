// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Delegates/IDelegateInstance.h"  // FDelegateHandle (5.8: Misc/DelegateHandle.h 已并入)
#include "ZZZTeamPanelEntryWidget.generated.h"

struct FOnAttributeChangeData;
class AZZZCharacter;
class UAbilitySystemComponent;

/**
 * 单槽子状态栏（左=名字/头像占位，右=上血条下能量条）。
 *
 * 分工（同 UZZZDamageNumberWidget 先例）：
 *   C++ — 绑定成员 ASC 的 Health/Energy 值变化 delegate（不轮询），把语义数据
 *         （百分比 / 高亮 / 灰显）推给 BP。绑定后立即手动拉一次现值（delegate
 *         只在变化时触发——首帧空白防护，UI-Design §一.2）。
 *   BP (WBP_TeamPanelEntry) — 全部展示：名字文本、血条/能量条 SetPercent、
 *         高亮描边与灰显样式，通过 BP_On* 事件接收。
 *
 * 槽实例由 UZZZTeamPanelWidget 按 roster 大小运行期创建；BindMember 每次
 * RefreshSquad 重绑（先解绑旧 delegate，防旧角色 delegate 残留）。
 * Member == nullptr（该职业尚未登场过一次）→ 只推名字，栏保持 BP 默认空态。
 */
UCLASS()
class UZZZTeamPanelEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 绑定一个 roster 槽位。Member 为空 = 未登场占位；非空 = 绑其 ASC 并立即拉现值。 */
	void BindMember(AZZZCharacter* InMember, bool bInIsCurrent, const FText& InDisplayName);

	/** 解绑 ASC 属性 delegate（重绑/重建前调用）。 */
	void UnbindMember();

	// === BP 展示入口（WBP_TeamPanelEntry 实现） ===

	/** 每次绑定时推一次：槽位角色名 + 是否当前操作（槽 0 → 高亮）+ 头像贴图（未配 → null）。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ZZZ|HUD")
	void BP_OnMemberBound(const FText& DisplayName, bool bIsCurrent, UTexture2D* Portrait);

	/** 血条百分比（0–1）+ 绝对值（参考图 "8604/8604" 式数值文本；BP 可只用百分比）。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ZZZ|HUD")
	void BP_OnHealthUpdated(float HealthPercent, float CurrentHealth, float MaxHealth);

	/** 能量条百分比（0–1；隐藏队员后台回能照常反映）。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ZZZ|HUD")
	void BP_OnEnergyUpdated(float EnergyPercent);

	/** 灰显切换（HP==0 事件驱动，见 UpdateHealth 注释）。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ZZZ|HUD")
	void BP_OnEliminatedUpdated(bool bEliminated);

private:
	void UpdateHealth(float NewHealth);
	void UpdateEnergy(float NewEnergy);

	void OnHealthChanged(const FOnAttributeChangeData& Data);
	void OnEnergyChanged(const FOnAttributeChangeData& Data);

	/** 本槽绑定的成员（隐藏中/操作中都绑——面板显示活体实例的实时属性）。 */
	TWeakObjectPtr<AZZZCharacter> BoundMember;

	FDelegateHandle HealthChangeHandle;
	FDelegateHandle EnergyChangeHandle;

	/** 本槽是否当前操作角色（槽 0，高亮由 BP 依据 BP_OnMemberBound 参数实现）。 */
	bool bIsCurrentSlot = false;

	/** 灰显状态缓存——只在翻转时推一次 BP_OnEliminatedUpdated。 */
	bool bIsEliminated = false;
};
