// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZTeamPanelWidget.h"

#include "Components/PanelWidget.h"
#include "Player/ZZZPlayerController.h"
#include "ZZZCharacter.h"
#include "ZZZCombatRemake.h"
#include "ZZZTeamPanelEntryWidget.h"

void UZZZTeamPanelWidget::RefreshSquad(AZZZPlayerController* PC, AZZZCharacter* CurrentPawn)
{
	if (!PC)
	{
		return;
	}

	if (!EntryContainer)
	{
		UE_LOG(LogZZZCombatRemake, Warning,
			TEXT("RefreshSquad: EntryContainer not found (name the panel hosting WBP_TeamPanelEntry 'EntryContainer')."));
		return;
	}

	// 面板本体 = EntryContainer 里的第一个 UZZZTeamPanelEntryWidget（3 栏位做死在一块，
	// 2026-09-09 重构后不再按槽位复用）。
	UZZZTeamPanelEntryWidget* Panel = nullptr;
	for (int32 ChildIndex = 0; ChildIndex < EntryContainer->GetChildrenCount(); ++ChildIndex)
	{
		if (UZZZTeamPanelEntryWidget* Candidate =
				Cast<UZZZTeamPanelEntryWidget>(EntryContainer->GetChildAt(ChildIndex)))
		{
			Panel = Candidate;
			break;
		}
	}

	if (!Panel)
	{
		UE_LOG(LogZZZCombatRemake, Warning,
			TEXT("RefreshSquad: EntryContainer 里没有 WBP_TeamPanelEntry 实例——小队栏不显示。"));
		return;
	}

	// 当前操作角色在轮转序的下标。不在 roster（INDEX_NONE，理论不发生的调试态）
	// → 槽序从 0 平铺；正常路径槽 0 = 当前（高亮）、槽 1/2 = 后续切出。
	const int32 RosterSize = PC->GetSquadRosterSize();
	const int32 CurrentIndex = PC->GetCurrentSquadIndex(CurrentPawn);
	if (CurrentPawn && CurrentIndex == INDEX_NONE)
	{
		UE_LOG(LogZZZCombatRemake, Warning,
			TEXT("RefreshSquad: current pawn '%s' is not in SquadClasses — slots shown unrotated."),
			*GetNameSafe(CurrentPawn));
	}

	for (int32 SlotIndex = 0; SlotIndex < UZZZTeamPanelEntryWidget::SlotCount; ++SlotIndex)
	{
		// roster 人数 < 栏位数 → 多余栏位整槽隐藏（N<3 不显示空槽）。
		if (SlotIndex >= RosterSize)
		{
			Panel->ClearSlot(SlotIndex);
			continue;
		}

		// 槽 i = SquadClasses[(当前索引 + i) % N]（UI-Design §2.2 槽位序）。
		const int32 RosterIndex =
			(RosterSize + (CurrentIndex != INDEX_NONE ? CurrentIndex : 0) + SlotIndex) % RosterSize;
		Panel->BindSlot(SlotIndex, PC->GetSquadMemberByClass(PC->GetSquadRosterClass(RosterIndex)));
	}
}
