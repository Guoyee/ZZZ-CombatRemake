// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Delegates/IDelegateInstance.h"  // FDelegateHandle (5.8: Misc/DelegateHandle.h 已并入)
#include "ZZZTeamPanelEntryWidget.generated.h"

struct FOnAttributeChangeData;
class AZZZCharacter;
class UImage;
class USizeBox;
class UTextBlock;

/**
 * 左上小队状态栏本体（WBP_TeamPanelEntry）——**3 个栏位做死成一体的 ZZZ 样式面板**
 * （2026-09-09 重构：栏位不再复用，整块面板一个 widget）。
 *
 * 分工：
 *   C++ — 按控件名取到 3 组槽位控件（头像 / 血条 / 能量条）+ 当前角色数值文本，
 *         把成员数据直接写进控件。槽 0 = 当前操作角色，槽 1/2 = 后续切出（槽序
 *         由 UZZZTeamPanelWidget 旋转后传入）。
 *   BP (WBP_TeamPanelEntry) — 纯布局与样式；事件图无需任何逻辑。
 *
 * 控件命名（设计器里的名字 → C++ 用途；旧名做兼容回落）：
 *   | 名字 | 用途 | 兼容旧名 |
 *   |---|---|---|
 *   | `Avatar1/2/3` | 槽位头像（成员 BP 的 `PortraitTexture`；未配则隐藏） | — |
 *   | `HP1/HP2/HP3` | 槽位血条容器（SizeBox，子控件 = ProgressBar） | — |
 *   | `MP1/MP2/MP3` | 槽位能量条容器（同上） | `MP2_1`（槽 3 旧名） |
 *   | `HealthText` | 当前角色 HP 数值文本（"899/1000"，无千分位） | `TextBlock_172` |
 *   | `SlotBg1/2/3` | 槽位底板（空槽时隐藏） | `Image` / `Image_1` / `Image_3` |
 */
UCLASS()
class UZZZTeamPanelEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 固定槽位数——与 WBP_TeamPanelEntry 里摆的栏位数一致（做死，不随 roster 变化）。 */
	static constexpr int32 SlotCount = 3;

	/**
	 * 绑定槽位成员：解旧 delegate → 头像（有贴图才显示）→ 绑 HP/MP 值变化 delegate
	 * → 立即拉一次现值（防首帧空白）→ 槽 0 顺带刷新数值文本。
	 * @param Member 可为 nullptr（空槽：隐藏该槽数据控件）。
	 */
	void BindSlot(int32 SlotIndex, AZZZCharacter* Member);

	/** 清空槽位：解绑 delegate + 隐藏该槽全部控件（roster 人数 < 3 时由面板调用）。 */
	void ClearSlot(int32 SlotIndex);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	// === 控件缓存（NativeConstruct 按名字解析；缺哪个只影响对应显示并打 Warning） ===
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> Avatars;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USizeBox>> HPBoxes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USizeBox>> MPBoxes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> SlotBackgrounds;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HealthText;

	// === 每槽绑定状态 ===
	TWeakObjectPtr<AZZZCharacter> BoundMembers[SlotCount];
	FDelegateHandle HealthHandles[SlotCount];
	FDelegateHandle EnergyHandles[SlotCount];

	void ResolveWidgets();
	void UnbindSlot(int32 SlotIndex);
	void SetSlotVisible(int32 SlotIndex, bool bVisible);
	void UpdateHealth(int32 SlotIndex, float NewHealth);
	void UpdateEnergy(int32 SlotIndex, float NewEnergy);
	void RefreshHealthText();
};
