// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ZZZTeamPanelWidget.generated.h"

class AZZZCharacter;
class AZZZPlayerController;
class UPanelWidget;

/**
 * 左上小队状态栏（横排子状态栏 ×N）。
 *
 * 分工：
 *   C++ — 槽位 = `EntryContainer` 里**设计器预放的** UZZZTeamPanelEntryWidget
 *         子控件（顺序即槽序）。RefreshSquad（每次 Possess 调用，UI-Design §2.2
 *         = Task #6 本体）按"槽 i = 当前角色后第 i 位"旋转重绑各槽数据 + 高亮
 *         槽 0；roster 人数 < 预放槽数 → 多余槽 Collapsed。
 *   BP (WBP_TeamPanel) — 布局：任意 Panel 命名 "EntryContainer"，里面摆好
 *         WBP_TeamPanelEntry 实例——位置/间距/缩放全在设计器里拖，所见即所得。
 *
 * 轮转序与切人共用 PC SquadClasses（轮转序真源，UI-Design §一.5）：槽 i 的角色
 * = SquadClasses[(当前索引 + i) % N]；成员实例按类查找（SquadMembers 非 index 对齐）。
 */
UCLASS()
class UZZZTeamPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 每次 Possess 后由 UZZZPlayerHUDWidget 调用：槽序旋转 + 高亮 + 重绑。 */
	void RefreshSquad(AZZZPlayerController* PC, AZZZCharacter* CurrentPawn);

protected:
	/**
	 * 槽容器（HBox / Canvas / Overlay 等任意 Panel 均可）——其子控件按顺序当槽位。
	 * 条目由设计器在 WBP_TeamPanel 里摆放（运行时不再创建）。
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> EntryContainer;
};
