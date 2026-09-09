// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZTeamPanelEntryWidget.h"

#include "AbilitySystemComponent.h"
#include "Attributes/ZZZAttributeSet.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "GameplayEffectTypes.h"  // FOnAttributeChangeData (Data.NewValue)
#include "ZZZCharacter.h"
#include "ZZZCombatRemake.h"

namespace
{
	/** 取槽位数据条：容器（SizeBox）的唯一子控件就是 ProgressBar。 */
	UProgressBar* GetBarFromBox(const USizeBox* Box)
	{
		return Box ? Cast<UProgressBar>(Box->GetChildAt(0)) : nullptr;
	}

	/** 槽位控件名（主名；旧名回落见 ResolveWidgets 的兼容表）。 */
	void GetSlotWidgetNames(int32 SlotIndex, FName& OutAvatar, FName& OutHP, FName& OutMP,
		FName& OutSlotBg)
	{
		static const TCHAR* AvatarNames[] = { TEXT("Avatar1"), TEXT("Avatar2"), TEXT("Avatar3") };
		static const TCHAR* HpNames[] = { TEXT("HP1"), TEXT("HP2"), TEXT("HP3") };
		static const TCHAR* MpNames[] = { TEXT("MP1"), TEXT("MP2"), TEXT("MP3") };
		static const TCHAR* SlotBgNames[] = { TEXT("SlotBg1"), TEXT("SlotBg2"), TEXT("SlotBg3") };

		const int32 Index = FMath::Clamp(SlotIndex, 0, UZZZTeamPanelEntryWidget::SlotCount - 1);
		OutAvatar = FName(AvatarNames[Index]);
		OutHP = FName(HpNames[Index]);
		OutMP = FName(MpNames[Index]);
		OutSlotBg = FName(SlotBgNames[Index]);
	}
}

void UZZZTeamPanelEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ResolveWidgets();
}

void UZZZTeamPanelEntryWidget::NativeDestruct()
{
	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		UnbindSlot(SlotIndex);
	}
	Super::NativeDestruct();
}

void UZZZTeamPanelEntryWidget::ResolveWidgets()
{
	if (Avatars.Num() == SlotCount)
	{
		return;  // 已解析
	}

	Avatars.SetNum(SlotCount);
	HPBoxes.SetNum(SlotCount);
	MPBoxes.SetNum(SlotCount);
	SlotBackgrounds.SetNum(SlotCount);

	// 兼容旧名的回落表（只在主名找不到时用）。
	static const TCHAR* MpLegacy[] = { nullptr, nullptr, TEXT("MP2_1") };
	static const TCHAR* BgLegacy[] = { TEXT("Image"), TEXT("Image_1"), TEXT("Image_3") };

	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		FName AvatarName, HPName, MPName, SlotBgName;
		GetSlotWidgetNames(SlotIndex, AvatarName, HPName, MPName, SlotBgName);

		Avatars[SlotIndex] = Cast<UImage>(GetWidgetFromName(AvatarName));
		HPBoxes[SlotIndex] = Cast<USizeBox>(GetWidgetFromName(HPName));

		MPBoxes[SlotIndex] = Cast<USizeBox>(GetWidgetFromName(MPName));
		if (!MPBoxes[SlotIndex] && MpLegacy[SlotIndex])
		{
			MPBoxes[SlotIndex] = Cast<USizeBox>(GetWidgetFromName(FName(MpLegacy[SlotIndex])));
		}

		SlotBackgrounds[SlotIndex] = Cast<UImage>(GetWidgetFromName(SlotBgName));
		if (!SlotBackgrounds[SlotIndex] && BgLegacy[SlotIndex])
		{
			SlotBackgrounds[SlotIndex] = Cast<UImage>(GetWidgetFromName(FName(BgLegacy[SlotIndex])));
		}

		if (!Avatars[SlotIndex] || !HPBoxes[SlotIndex] || !MPBoxes[SlotIndex])
		{
			UE_LOG(LogZZZCombatRemake, Warning,
				TEXT("WBP_TeamPanelEntry: 槽 %d 控件缺失（需要 Avatar%d / HP%d / MP%d）——该槽数据不会显示。"),
				SlotIndex, SlotIndex + 1, SlotIndex + 1, SlotIndex + 1);
		}
	}

	HealthText = Cast<UTextBlock>(GetWidgetFromName(TEXT("HealthText")));
	if (!HealthText)
	{
		HealthText = Cast<UTextBlock>(GetWidgetFromName(TEXT("TextBlock_172")));
	}
	if (!HealthText)
	{
		UE_LOG(LogZZZCombatRemake, Warning,
			TEXT("WBP_TeamPanelEntry: 找不到数值文本控件（命名 HealthText）——HP 数值不显示。"));
	}
}

void UZZZTeamPanelEntryWidget::BindSlot(int32 SlotIndex, AZZZCharacter* Member)
{
	if (!Avatars.IsValidIndex(SlotIndex))
	{
		return;
	}

	UnbindSlot(SlotIndex);
	BoundMembers[SlotIndex] = Member;

	// 头像：成员配了 PortraitTexture 才显示（未配/空槽 → 隐藏）。
	if (UImage* Avatar = Avatars[SlotIndex])
	{
		if (UTexture2D* Portrait = Member ? Member->GetPortraitTexture() : nullptr)
		{
			Avatar->SetBrushFromTexture(Portrait, /*bMatchSize=*/false);
			Avatar->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Avatar->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	// 空槽：数据条隐藏，数值文本交给槽 0 的逻辑处理。
	if (!Member)
	{
		if (USizeBox* HPBox = HPBoxes[SlotIndex]) { HPBox->SetVisibility(ESlateVisibility::Hidden); }
		if (USizeBox* MPBox = MPBoxes[SlotIndex]) { MPBox->SetVisibility(ESlateVisibility::Hidden); }
		if (SlotIndex == 0) { RefreshHealthText(); }
		return;
	}

	// 有成员：数据条显示 + 绑值变化 delegate + 立即拉一次现值（防首帧空白）。
	if (USizeBox* HPBox = HPBoxes[SlotIndex]) { HPBox->SetVisibility(ESlateVisibility::HitTestInvisible); }
	if (USizeBox* MPBox = MPBoxes[SlotIndex]) { MPBox->SetVisibility(ESlateVisibility::HitTestInvisible); }

	UAbilitySystemComponent* MemberASC = Member->GetAbilitySystemComponent();
	if (!MemberASC)
	{
		return;
	}

	// lambda 绑定槽号（该委托类型没有带 payload 的 AddUObject 重载）；
	// 句柄存起来供 UnbindSlot/NativeDestruct 精确移除。
	HealthHandles[SlotIndex] = MemberASC->GetGameplayAttributeValueChangeDelegate(
		UZZZAttributeSet::GetHealthAttribute())
		.AddLambda([this, SlotIndex](const FOnAttributeChangeData& Data)
		{
			UpdateHealth(SlotIndex, Data.NewValue);
		});
	EnergyHandles[SlotIndex] = MemberASC->GetGameplayAttributeValueChangeDelegate(
		UZZZAttributeSet::GetEnergyAttribute())
		.AddLambda([this, SlotIndex](const FOnAttributeChangeData& Data)
		{
			UpdateEnergy(SlotIndex, Data.NewValue);
		});

	UpdateHealth(SlotIndex, MemberASC->GetNumericAttribute(UZZZAttributeSet::GetHealthAttribute()));
	UpdateEnergy(SlotIndex, MemberASC->GetNumericAttribute(UZZZAttributeSet::GetEnergyAttribute()));
}

void UZZZTeamPanelEntryWidget::ClearSlot(int32 SlotIndex)
{
	if (!Avatars.IsValidIndex(SlotIndex))
	{
		return;
	}

	UnbindSlot(SlotIndex);
	SetSlotVisible(SlotIndex, /*bVisible=*/false);
}

void UZZZTeamPanelEntryWidget::UnbindSlot(int32 SlotIndex)
{
	if (!Avatars.IsValidIndex(SlotIndex))
	{
		return;
	}

	if (AZZZCharacter* Member = BoundMembers[SlotIndex].Get())
	{
		if (UAbilitySystemComponent* MemberASC = Member->GetAbilitySystemComponent())
		{
			if (HealthHandles[SlotIndex].IsValid())
			{
				MemberASC->GetGameplayAttributeValueChangeDelegate(
					UZZZAttributeSet::GetHealthAttribute()).Remove(HealthHandles[SlotIndex]);
			}
			if (EnergyHandles[SlotIndex].IsValid())
			{
				MemberASC->GetGameplayAttributeValueChangeDelegate(
					UZZZAttributeSet::GetEnergyAttribute()).Remove(EnergyHandles[SlotIndex]);
			}
		}
	}

	HealthHandles[SlotIndex].Reset();
	EnergyHandles[SlotIndex].Reset();
	BoundMembers[SlotIndex].Reset();
}

void UZZZTeamPanelEntryWidget::SetSlotVisible(int32 SlotIndex, bool bVisible)
{
	const ESlateVisibility SlotVisibility =
		bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden;

	if (Avatars.IsValidIndex(SlotIndex))
	{
		if (UImage* Avatar = Avatars[SlotIndex]) { Avatar->SetVisibility(SlotVisibility); }
		if (UImage* SlotBg = SlotBackgrounds[SlotIndex]) { SlotBg->SetVisibility(SlotVisibility); }
		if (USizeBox* HPBox = HPBoxes[SlotIndex]) { HPBox->SetVisibility(SlotVisibility); }
		if (USizeBox* MPBox = MPBoxes[SlotIndex]) { MPBox->SetVisibility(SlotVisibility); }
	}

	// 槽 0 被清空（理论上不发生——当前角色必在队）时数值文本一并清掉。
	if (SlotIndex == 0 && HealthText)
	{
		HealthText->SetVisibility(SlotVisibility);
	}
}

void UZZZTeamPanelEntryWidget::UpdateHealth(int32 SlotIndex, float NewHealth)
{
	if (!Avatars.IsValidIndex(SlotIndex))
	{
		return;
	}

	AZZZCharacter* Member = BoundMembers[SlotIndex].Get();
	UAbilitySystemComponent* MemberASC = Member ? Member->GetAbilitySystemComponent() : nullptr;
	const float MaxHealth = MemberASC
		? MemberASC->GetNumericAttribute(UZZZAttributeSet::GetMaxHealthAttribute())
		: 0.0f;

	if (UProgressBar* Bar = GetBarFromBox(HPBoxes[SlotIndex]))
	{
		Bar->SetPercent(MaxHealth > 0.0f ? FMath::Clamp(NewHealth / MaxHealth, 0.0f, 1.0f) : 0.0f);
	}

	// 数值文本只跟当前操作角色（槽 0）——与参考图一致。
	if (SlotIndex == 0)
	{
		RefreshHealthText();
	}
}

void UZZZTeamPanelEntryWidget::UpdateEnergy(int32 SlotIndex, float NewEnergy)
{
	if (!Avatars.IsValidIndex(SlotIndex))
	{
		return;
	}

	AZZZCharacter* Member = BoundMembers[SlotIndex].Get();
	UAbilitySystemComponent* MemberASC = Member ? Member->GetAbilitySystemComponent() : nullptr;
	const float MaxEnergy = MemberASC
		? MemberASC->GetNumericAttribute(UZZZAttributeSet::GetMaxEnergyAttribute())
		: 0.0f;

	if (UProgressBar* Bar = GetBarFromBox(MPBoxes[SlotIndex]))
	{
		Bar->SetPercent(MaxEnergy > 0.0f ? FMath::Clamp(NewEnergy / MaxEnergy, 0.0f, 1.0f) : 0.0f);
	}
}

void UZZZTeamPanelEntryWidget::RefreshHealthText()
{
	if (!HealthText)
	{
		return;
	}

	AZZZCharacter* Member = BoundMembers[0].Get();
	UAbilitySystemComponent* MemberASC = Member ? Member->GetAbilitySystemComponent() : nullptr;
	if (!MemberASC)
	{
		HealthText->SetText(FText::GetEmpty());
		return;
	}

	const int32 Current = FMath::RoundToInt(
		MemberASC->GetNumericAttribute(UZZZAttributeSet::GetHealthAttribute()));
	const int32 Max = FMath::RoundToInt(
		MemberASC->GetNumericAttribute(UZZZAttributeSet::GetMaxHealthAttribute()));

	// 无千分位（对齐参考图 "8604/8604"）。
	const FNumberFormattingOptions& NoGrouping = FNumberFormattingOptions::DefaultNoGrouping();
	HealthText->SetText(FText::Format(
		NSLOCTEXT("ZZZ", "SquadHealthFormat", "{0}/{1}"),
		FText::AsNumber(Current, &NoGrouping),
		FText::AsNumber(Max, &NoGrouping)));
}
