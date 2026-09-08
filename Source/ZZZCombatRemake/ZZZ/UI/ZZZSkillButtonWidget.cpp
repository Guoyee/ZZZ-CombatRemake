// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZSkillButtonWidget.h"

#include "AbilitySystemComponent.h"
#include "Attributes/ZZZAttributeSet.h"
#include "GameplayEffectTypes.h"  // FOnAttributeChangeData (Data.NewValue)
#include "ZZZCharacter.h"

void UZZZSkillButtonWidget::SetBoundMember(AZZZCharacter* InMember)
{
	// 同角色幂等：切换回同一人物时 Possess 会触发重绑，但 delegate 本就绑着它。
	if (BoundMember.Get() == InMember)
	{
		return;
	}

	UnbindMember();
	BoundMember = InMember;

	if (!BoundMember.IsValid())
	{
		bIsReady = false;
		BP_OnReadyChanged(false);
		return;
	}

	UAbilitySystemComponent* MemberASC = BoundMember->GetAbilitySystemComponent();
	if (!MemberASC)
	{
		bIsReady = false;
		BP_OnReadyChanged(false);
		return;
	}

	EnergyChangeHandle = MemberASC->GetGameplayAttributeValueChangeDelegate(
		UZZZAttributeSet::GetEnergyAttribute())
		.AddUObject(this, &UZZZSkillButtonWidget::OnEnergyChanged);

	// 绑定后强制推一次现值：缓存来自上一个成员，只比较翻转会在"就绪 → 未就绪"
	// 的切人方向漏推（按钮保持上一人的彩色）。
	bIsReady = MemberASC->GetNumericAttribute(UZZZAttributeSet::GetEnergyAttribute())
		>= SpecialEnergyCost;
	BP_OnReadyChanged(bIsReady);
}

void UZZZSkillButtonWidget::UnbindMember()
{
	if (BoundMember.IsValid())
	{
		if (UAbilitySystemComponent* MemberASC = BoundMember->GetAbilitySystemComponent())
		{
			if (EnergyChangeHandle.IsValid())
			{
				MemberASC->GetGameplayAttributeValueChangeDelegate(
					UZZZAttributeSet::GetEnergyAttribute()).Remove(EnergyChangeHandle);
				EnergyChangeHandle.Reset();
			}
		}
	}

	BoundMember.Reset();
	bIsReady = false;
}

void UZZZSkillButtonWidget::OnEnergyChanged(const FOnAttributeChangeData& Data)
{
	const bool bNowReady = Data.NewValue >= SpecialEnergyCost;
	if (bNowReady == bIsReady)
	{
		return;  // 只在就绪态翻转时推 BP（无能量条, 逐点刷新无意义）
	}

	bIsReady = bNowReady;
	BP_OnReadyChanged(bIsReady);
}
