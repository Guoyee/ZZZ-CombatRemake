// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZPlayerHUDWidget.h"

#include "Player/ZZZPlayerController.h"
#include "ZZZSkillButtonWidget.h"
#include "ZZZTeamPanelWidget.h"

void UZZZPlayerHUDWidget::RefreshSquad(AZZZPlayerController* PC, AZZZCharacter* CurrentPawn)
{
	if (TeamPanel)
	{
		TeamPanel->RefreshSquad(PC, CurrentPawn);
	}

	// 技能按钮绑操作角色（Possessed pawn）——每次 Possess 重绑（SetBoundMember 内
	// 先解旧角色 delegate，同角色幂等）。
	if (SkillButton)
	{
		SkillButton->SetBoundMember(CurrentPawn);
	}
}
