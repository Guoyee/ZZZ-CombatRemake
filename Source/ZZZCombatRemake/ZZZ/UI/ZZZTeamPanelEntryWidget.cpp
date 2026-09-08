// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZTeamPanelEntryWidget.h"

#include "AbilitySystemComponent.h"
#include "Attributes/ZZZAttributeSet.h"
#include "GameplayEffectTypes.h"  // FOnAttributeChangeData (Data.NewValue)
#include "ZZZCharacter.h"

void UZZZTeamPanelEntryWidget::BindMember(
	AZZZCharacter* InMember, bool bInIsCurrent, const FText& InDisplayName)
{
	UnbindMember();

	bIsCurrentSlot = bInIsCurrent;
	BoundMember = InMember;

	// 名字 + 头像先推——BP 首帧按名字/头像布局；未登场成员（无 ASC 可绑）栏保持 BP 默认空态。
	BP_OnMemberBound(InDisplayName, bInIsCurrent,
		InMember ? InMember->GetPortraitTexture() : nullptr);

	if (!BoundMember.IsValid())
	{
		// 未登场成员（尚未 spawn，无 ASC 可绑）→ 推零值，槽位显示空条 + "0/0"，
		// 不留上一槽或设计器的残留数值（不调 UpdateHealth——避免把空槽判成"已阵亡"灰显）。
		BP_OnHealthUpdated(0.f, 0.f, 0.f);
		BP_OnEnergyUpdated(0.f);
		return;
	}

	UAbilitySystemComponent* MemberASC = BoundMember->GetAbilitySystemComponent();
	if (!MemberASC)
	{
		return;
	}

	HealthChangeHandle = MemberASC->GetGameplayAttributeValueChangeDelegate(
		UZZZAttributeSet::GetHealthAttribute())
		.AddUObject(this, &UZZZTeamPanelEntryWidget::OnHealthChanged);
	EnergyChangeHandle = MemberASC->GetGameplayAttributeValueChangeDelegate(
		UZZZAttributeSet::GetEnergyAttribute())
		.AddUObject(this, &UZZZTeamPanelEntryWidget::OnEnergyChanged);

	// 绑定后立即拉一次现值（delegate 只在变化时触发——首帧空白防护）。
	// 顺势触发 BP_OnEliminatedUpdated：若该成员初始即 HP==0（复用场上的战损实例）。
	UpdateHealth(MemberASC->GetNumericAttribute(UZZZAttributeSet::GetHealthAttribute()));
	UpdateEnergy(MemberASC->GetNumericAttribute(UZZZAttributeSet::GetEnergyAttribute()));
}

void UZZZTeamPanelEntryWidget::UnbindMember()
{
	if (BoundMember.IsValid())
	{
		if (UAbilitySystemComponent* MemberASC = BoundMember->GetAbilitySystemComponent())
		{
			if (HealthChangeHandle.IsValid())
			{
				MemberASC->GetGameplayAttributeValueChangeDelegate(
					UZZZAttributeSet::GetHealthAttribute()).Remove(HealthChangeHandle);
				HealthChangeHandle.Reset();
			}
			if (EnergyChangeHandle.IsValid())
			{
				MemberASC->GetGameplayAttributeValueChangeDelegate(
					UZZZAttributeSet::GetEnergyAttribute()).Remove(EnergyChangeHandle);
				EnergyChangeHandle.Reset();
			}
		}
	}

	BoundMember.Reset();
	bIsEliminated = false;
}

void UZZZTeamPanelEntryWidget::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	UpdateHealth(Data.NewValue);
}

void UZZZTeamPanelEntryWidget::OnEnergyChanged(const FOnAttributeChangeData& Data)
{
	UpdateEnergy(Data.NewValue);
}

void UZZZTeamPanelEntryWidget::UpdateHealth(float NewHealth)
{
	if (!BoundMember.IsValid())
	{
		return;
	}

	UAbilitySystemComponent* MemberASC = BoundMember->GetAbilitySystemComponent();
	if (!MemberASC)
	{
		return;
	}

	const float MaxHealth =
		MemberASC->GetNumericAttribute(UZZZAttributeSet::GetMaxHealthAttribute());
	BP_OnHealthUpdated(
		MaxHealth > 0.f ? FMath::Clamp(NewHealth / MaxHealth, 0.f, 1.f) : 0.f,
		NewHealth,
		MaxHealth);

	// 灰显判据 = HP==0（UI-Design §2.2：死亡必经伤害归零、事件触发；不用
	// State.Dead tag——防未来"非伤害归零"改动只需换这一处）。只在此翻转时广播。
	const bool bNowEliminated = NewHealth <= 0.f;
	if (bNowEliminated != bIsEliminated)
	{
		bIsEliminated = bNowEliminated;
		BP_OnEliminatedUpdated(bIsEliminated);
	}
}

void UZZZTeamPanelEntryWidget::UpdateEnergy(float NewEnergy)
{
	if (!BoundMember.IsValid())
	{
		return;
	}

	UAbilitySystemComponent* MemberASC = BoundMember->GetAbilitySystemComponent();
	if (!MemberASC)
	{
		return;
	}

	const float MaxEnergy =
		MemberASC->GetNumericAttribute(UZZZAttributeSet::GetMaxEnergyAttribute());
	BP_OnEnergyUpdated(
		MaxEnergy > 0.f ? FMath::Clamp(NewEnergy / MaxEnergy, 0.f, 1.f) : 0.f);
}
