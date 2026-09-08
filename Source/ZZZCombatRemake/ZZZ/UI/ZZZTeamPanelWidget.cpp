// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZTeamPanelWidget.h"

#include "Player/ZZZPlayerController.h"
#include "ZZZCharacter.h"
#include "ZZZCombatRemake.h"
#include "ZZZTeamPanelEntryWidget.h"

namespace
{
	/** 职业类 → 槽位显示名（占位头像=名字 Text，首版兜底）：剥 BP_ 资产前缀。 */
	FText GetCharacterDisplayName(UClass* CharacterClass)
	{
		if (!CharacterClass)
		{
			return FText::GetEmpty();
		}

		FString Name = CharacterClass->GetName();
		Name.RemoveFromStart(TEXT("BP_"));
		return FText::FromString(Name);
	}
}

void UZZZTeamPanelWidget::RefreshSquad(AZZZPlayerController* PC, AZZZCharacter* CurrentPawn)
{
	if (!PC)
	{
		return;
	}

	const int32 RosterSize = PC->GetSquadRosterSize();
	RebuildEntries(PC, RosterSize);

	// 当前操作角色在轮转序的下标。不在 roster（INDEX_NONE，理论不发生的调试态）
	// → 槽序从 0 平铺、无高亮；正常路径槽 0 = 当前（高亮）、槽 1 = 下一次切出。
	const int32 CurrentIndex = PC->GetCurrentSquadIndex(CurrentPawn);
	if (CurrentPawn && CurrentIndex == INDEX_NONE)
	{
		UE_LOG(LogZZZCombatRemake, Warning,
			TEXT("RefreshSquad: current pawn '%s' is not in SquadClasses — slots shown unrotated."),
			*GetNameSafe(CurrentPawn));
	}

	for (int32 SlotIndex = 0; SlotIndex < RosterSize; ++SlotIndex)
	{
		UZZZTeamPanelEntryWidget* Entry =
			Entries.IsValidIndex(SlotIndex) ? Entries[SlotIndex] : nullptr;
		if (!Entry)
		{
			continue;
		}

		// 槽 i = SquadClasses[(当前索引 + i) % N]（UI-Design §2.2 槽位序）。
		const int32 RosterIndex =
			(RosterSize + (CurrentIndex != INDEX_NONE ? CurrentIndex : 0) + SlotIndex) % RosterSize;
		UClass* RosterClass = PC->GetSquadRosterClass(RosterIndex);
		AZZZCharacter* Member = PC->GetSquadMemberByClass(RosterClass);

		Entry->BindMember(
			Member,
			/*bIsCurrent=*/RosterIndex == CurrentIndex && CurrentIndex != INDEX_NONE,
			GetCharacterDisplayName(RosterClass));
	}
}

void UZZZTeamPanelWidget::RebuildEntries(AZZZPlayerController* PC, int32 RosterSize)
{
	// 大小不变 → 复用实例（槽视觉位置稳定，只重绑数据）。
	if (Entries.Num() == RosterSize)
	{
		return;
	}

	// 清空旧槽（roster 配置变化时）。
	for (const TObjectPtr<UZZZTeamPanelEntryWidget>& Entry : Entries)
	{
		if (Entry)
		{
			Entry->UnbindMember();
			if (EntryContainer)
			{
				EntryContainer->RemoveChild(Entry);
			}
		}
	}
	Entries.Reset();

	if (RosterSize <= 0)
	{
		return;
	}

	if (!EntryWidgetClass)
	{
		UE_LOG(LogZZZCombatRemake, Warning,
			TEXT("RebuildEntries: EntryWidgetClass not configured (WBP_TeamPanelEntry) — team panel empty."));
		return;
	}
	if (!EntryContainer)
	{
		UE_LOG(LogZZZCombatRemake, Warning,
			TEXT("RebuildEntries: EntryContainer not found (name the HorizontalBox 'EntryContainer' in WBP_TeamPanel)."));
		return;
	}

	// 运行期创建 N 个子状态栏并按槽序加入（空槽规则：N<3 时本就只建 N 个）。
	for (int32 i = 0; i < RosterSize; ++i)
	{
		UZZZTeamPanelEntryWidget* Entry =
			CreateWidget<UZZZTeamPanelEntryWidget>(PC, EntryWidgetClass);
		if (Entry)
		{
			EntryContainer->AddChild(Entry);
			Entries.Add(Entry);
		}
	}
}
