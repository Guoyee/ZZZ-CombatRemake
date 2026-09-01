// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZDamageNumberWidget.h"

void UZZZDamageNumberWidget::SetDamageValue(float Damage, FGameplayTag ElementTag)
{
	// All presentation lives in the WBP implementation.
	BP_OnDamageShown(Damage, ElementTag);
}

FLinearColor UZZZDamageNumberWidget::GetElementColor(FGameplayTag ElementTag)
{
	// Phase 4 extension point: map Element.Fire / Ice / Electric / Ether to
	// distinct colors here — the rest of the pipeline stays untouched.
	// Today every hit is physical → default red.
	return FLinearColor::Red;
}
