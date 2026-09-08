// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZEnemyHeadWidget.h"

#include "AbilitySystemComponent.h"
#include "Attributes/ZZZAttributeSet.h"
#include "GameplayEffectTypes.h"  // FOnAttributeChangeData (Data.NewValue)

void UZZZEnemyHeadWidget::BindStatus(UAbilitySystemComponent* InASC, const FText& InDisplayName)
{
	UnbindStatus();

	BoundASC = InASC;

	// 名字先推——BP 首帧按名字决定名字区显隐（空名由 BP 折叠，UI-Design §三）。
	BP_OnBound(InDisplayName);

	if (!BoundASC.IsValid())
	{
		return;
	}

	HealthChangeHandle = BoundASC->GetGameplayAttributeValueChangeDelegate(
		UZZZAttributeSet::GetHealthAttribute())
		.AddUObject(this, &UZZZEnemyHeadWidget::OnHealthChanged);
	DazeChangeHandle = BoundASC->GetGameplayAttributeValueChangeDelegate(
		UZZZAttributeSet::GetDazeAttribute())
		.AddUObject(this, &UZZZEnemyHeadWidget::OnDazeChanged);

	// 绑定后立即拉一次现值（delegate 只在变化时触发——首帧空白防护）。
	UpdateHealth(BoundASC->GetNumericAttribute(UZZZAttributeSet::GetHealthAttribute()));
	UpdateDaze(BoundASC->GetNumericAttribute(UZZZAttributeSet::GetDazeAttribute()));
}

void UZZZEnemyHeadWidget::UnbindStatus()
{
	if (BoundASC.IsValid())
	{
		if (HealthChangeHandle.IsValid())
		{
			BoundASC->GetGameplayAttributeValueChangeDelegate(
				UZZZAttributeSet::GetHealthAttribute()).Remove(HealthChangeHandle);
			HealthChangeHandle.Reset();
		}
		if (DazeChangeHandle.IsValid())
		{
			BoundASC->GetGameplayAttributeValueChangeDelegate(
				UZZZAttributeSet::GetDazeAttribute()).Remove(DazeChangeHandle);
			DazeChangeHandle.Reset();
		}
	}

	BoundASC.Reset();
}

void UZZZEnemyHeadWidget::NativeDestruct()
{
	UnbindStatus();
	Super::NativeDestruct();
}

void UZZZEnemyHeadWidget::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	UpdateHealth(Data.NewValue);
}

void UZZZEnemyHeadWidget::OnDazeChanged(const FOnAttributeChangeData& Data)
{
	UpdateDaze(Data.NewValue);
}

void UZZZEnemyHeadWidget::UpdateHealth(float NewHealth)
{
	if (!BoundASC.IsValid())
	{
		return;
	}

	const float MaxHealth =
		BoundASC->GetNumericAttribute(UZZZAttributeSet::GetMaxHealthAttribute());
	BP_OnHealthUpdated(
		MaxHealth > 0.f ? FMath::Clamp(NewHealth / MaxHealth, 0.f, 1.f) : 0.f);
}

void UZZZEnemyHeadWidget::UpdateDaze(float NewDaze)
{
	if (!BoundASC.IsValid())
	{
		return;
	}

	const float MaxDaze =
		BoundASC->GetNumericAttribute(UZZZAttributeSet::GetMaxDazeAttribute());
	BP_OnDazeUpdated(
		MaxDaze > 0.f ? FMath::Clamp(NewDaze / MaxDaze, 0.f, 1.f) : 0.f);
}
