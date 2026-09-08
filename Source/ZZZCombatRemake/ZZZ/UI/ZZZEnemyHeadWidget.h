// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Delegates/IDelegateInstance.h"  // FDelegateHandle (5.8: Misc/DelegateHandle.h 已并入)
#include "ZZZEnemyHeadWidget.generated.h"

struct FOnAttributeChangeData;
class UAbilitySystemComponent;

/**
 * 敌人头顶状态条（名字 + 血条 + 失衡条）——WBP_EnemyHead 的 C++ 侧逻辑件。
 *
 * 分工（同 UZZZTeamPanelEntryWidget 先例，UI-Design §三）：
 *   C++ — BindStatus 接收本敌 ASC + 配置名：绑 Health/Daze 值变化 delegate
 *         （不轮询；delegate 在 Instant 命中 / Duration / Infinite GE 变化时均触发，
 *         含本仓 PostGameplayEffectExecute 的容器 setter 路径——UI-Design §一.2），
 *         绑定后立即手动拉一次现值（首帧空白防护）。
 *   BP (WBP_EnemyHead) — 全部展示：名字文本（空串可折叠）、血条/失衡条 SetPercent
 *         （失衡条黄、与血条区分），通过 BP_On* 事件接收。
 *
 * 实例由 AZZZCombatEnemy::BeginPlay 按 HeadWidgetClass 创建（SetWidgetClass →
 * UWidgetComponent 即刻 CreateWidget），随敌人销毁自然销毁；NativeDestruct 解绑
 * delegate 防悬挂。HP==0 时组件整体隐藏由敌人侧 OnHealthChanged 做（UI-Design §3.2），
 * 本 widget 只管把 0% 推给血条。
 */
UCLASS()
class UZZZEnemyHeadWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 绑定本敌 ASC 的 Health/Daze 值变化 delegate；名字（敌人 BP DisplayName，可空）推给 BP。 */
	void BindStatus(UAbilitySystemComponent* InASC, const FText& InDisplayName);

	/** 解绑 ASC 属性 delegate（重绑/销毁前调用；NativeDestruct 自动调）。 */
	void UnbindStatus();

	// === BP 展示入口（WBP_EnemyHead 实现） ===

	/** 绑定瞬间推一次：显示名（空 = BP 自行折叠名字区）。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ZZZ|EnemyHead")
	void BP_OnBound(const FText& DisplayName);

	/** 血条百分比（0–1，随伤害下降）。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ZZZ|EnemyHead")
	void BP_OnHealthUpdated(float HealthPercent);

	/** 失衡条百分比（0–1，随失衡累计上涨；失衡触发归零随 SetDaze 事件自然反映）。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ZZZ|EnemyHead")
	void BP_OnDazeUpdated(float DazePercent);

protected:
	virtual void NativeDestruct() override;

private:
	void OnHealthChanged(const FOnAttributeChangeData& Data);
	void OnDazeChanged(const FOnAttributeChangeData& Data);

	void UpdateHealth(float NewHealth);
	void UpdateDaze(float NewDaze);

	/** 本条绑定的敌人 ASC（与敌人同生命周期；NativeDestruct 解绑防 teardown 悬挂）。 */
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;

	FDelegateHandle HealthChangeHandle;
	FDelegateHandle DazeChangeHandle;
};
