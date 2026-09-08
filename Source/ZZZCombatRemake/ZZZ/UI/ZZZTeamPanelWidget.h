// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/HorizontalBox.h"
#include "ZZZTeamPanelWidget.generated.h"

class AZZZCharacter;
class AZZZPlayerController;
class UZZZTeamPanelEntryWidget;

/**
 * 左上小队状态栏（横排子状态栏 ×N）。
 *
 * 分工：
 *   C++ — 槽数 = PC roster 大小（1–3，懒重建）；RefreshSquad（每次 Possess
 *         调用，UI-Design §2.2 = Task #6 本体）按"槽 i = 当前角色后第 i 位"
 *         旋转重绑各槽成员 + 高亮槽 0。BP 只提供布局（容器）+ 条目类。
 *   BP (WBP_TeamPanel) — 布局：命名 HorizontalBox "EntryContainer"；
 *         子状态栏类配在 EntryWidgetClass（WBP_TeamPanelEntry）。
 *
 * 轮转序与切人共用 PC SquadClasses（轮转序真源，UI-Design §一.5）：槽 i 的角色
 * = SquadClasses[(当前索引 + i) % N]；成员实例按类从 SquadMembers 查找
 * （不能假设 index 对齐——实例按"首次登场/注册"序追加）。
 */
UCLASS()
class UZZZTeamPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 每次 Possess 后由 UZZZPlayerHUDWidget 调用：槽序旋转 + 高亮 + 重绑。 */
	void RefreshSquad(AZZZPlayerController* PC, AZZZCharacter* CurrentPawn);

protected:
	/** 子状态栏 WBP 类（WBP_TeamPanelEntry）——运行期按 roster 大小实例化。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZZZ|HUD")
	TSubclassOf<UZZZTeamPanelEntryWidget> EntryWidgetClass;

	/** WBP_TeamPanel 内的横排容器——运行期条目按槽序 AddChild 至此。 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> EntryContainer;

	/** 现有槽实例（与 roster 槽一一对应；RefreshSquad 只重绑不重建）。 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UZZZTeamPanelEntryWidget>> Entries;

private:
	/** roster 大小变化时清空重建条目（大小不变 → 复用实例仅重绑）。 */
	void RebuildEntries(AZZZPlayerController* PC, int32 RosterSize);
};
