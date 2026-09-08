// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ZZZPlayerHUDWidget.generated.h"

class AZZZCharacter;
class AZZZPlayerController;
class UZZZSkillButtonWidget;
class UZZZTeamPanelWidget;

/**
 * 屏幕 HUD 根容器（WBP_ZZZHUD）——左上 TeamPanel + 右下技能按钮。
 *
 * 创建/生命周期由 AZZZPlayerController 管（CreateWidget + AddToViewport，
 * HUDWidgetClass 资产引用在 PC BP 填，同 DamageNumberWidgetClass 先例）。
 *
 * 分工：
 *   C++ — 只做语义转发：RefreshSquad(PC, CurrentPawn) 分发给 TeamPanel
 *         （槽序旋转/重绑）与技能按钮（操作角色重绑）。
 *   BP (WBP_ZZZHUD) — 布局：画布内放 WBP_TeamPanel（命名 "TeamPanel"，左上）
 *         与 WBP_ZZZSkillButton（命名 "SkillButton"，右下）。
 *
 * 本轮技能区只挂特殊技钮；大招槽 = 容器+可变子钮结构，Decibel 步再加（UI-Design §六）。
 */
UCLASS()
class UZZZPlayerHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 每次 Possess 后由 PC 调用（含切换/招架支援）；HUD 创建后立即调一次。 */
	UFUNCTION(BlueprintCallable, Category = "ZZZ|HUD")
	void RefreshSquad(AZZZPlayerController* PC, AZZZCharacter* CurrentPawn);

protected:
	/** 左上小队状态栏（WBP_TeamPanel——UZZZTeamPanelWidget 子类，BindWidgetOptional：删掉子件不崩、只缺功能）。 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UZZZTeamPanelWidget> TeamPanel;

	/** 右下特殊技按钮（WBP_ZZZSkillButton——UZZZSkillButtonWidget 子类）。 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UZZZSkillButtonWidget> SkillButton;
};
