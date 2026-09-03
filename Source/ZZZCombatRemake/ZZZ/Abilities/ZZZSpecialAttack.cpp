// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZSpecialAttack.h"
#include "AbilitySystemComponent.h"
#include "AbilityTask_RotateToTarget.h"
#include "Attributes/ZZZAttributeSet.h"
#include "Effects/ZZZEnergyGameplayEffects.h"
#include "Tags/ZZZGameplayTags.h"
#include "ZZZCombatRemake.h"

UZZZSpecialAttack::UZZZSpecialAttack()
{
}

UAnimMontage* UZZZSpecialAttack::ResolveLeadMontage() const
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return nullptr;
	}

	// 前驱 = 当前活动的战斗 GA(除特殊技自身)。激活同步性保证: 本 GA 激活瞬间
	// 前驱 GA 仍活动(其 EndAbility 要等本技蒙太奇打断它)——见类注释。
	// 只取第一个活动战斗 GA; 档位按 Direct > Combo > Dash 优先级匹配其资产 tag。
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.Ability || !Spec.IsActive()
			|| Spec.Ability->GetClass()->IsChildOf(UZZZSpecialAttack::StaticClass()))
		{
			continue;
		}
		if (!Spec.Ability->GetClass()->IsChildOf(UZZZGameplayAbility::StaticClass()))
		{
			continue;
		}

		const FGameplayTagContainer& AssetTags = Spec.Ability->GetAssetTags();

		// HasTagExact: 规则行是段位身份 tag(如 Ability.Attack.Basic.BasicAttack02),
		// 只要求精确命中, 不做父 tag 层级放宽。
		for (const FGameplayTag& Tag : DirectEntryContextTags)
		{
			if (Tag.IsValid() && AssetTags.HasTagExact(Tag))
			{
				return nullptr;  // 免起手直连主体
			}
		}
		for (const FGameplayTag& Tag : ComboLeadContextTags)
		{
			if (Tag.IsValid() && AssetTags.HasTagExact(Tag))
			{
				return LeadComboMontage;
			}
		}
		for (const FGameplayTag& Tag : DashLeadContextTags)
		{
			if (Tag.IsValid() && AssetTags.HasTagExact(Tag))
			{
				return LeadDashMontage;
			}
		}
		break;  // 只解析第一个活动前驱
	}

	// 自由态 / 收刀段(无活动前驱)或前驱未匹配任何规则 → A 档完整起手。
	return LeadFullMontage;
}

void UZZZSpecialAttack::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FZZZGameplayTags& GameplayTags = FZZZGameplayTags::Get();

	// 1) 主体版本分支留在 GA 内(能量判定, 输入门控不预选): Energy >= EnergyCost
	//    → 强化主体 + 扣费。起手 A/B/C 与版本无关、共用同一套。
	const float CurrentEnergy = ASC->GetNumericAttribute(UZZZAttributeSet::GetEnergyAttribute());
	const bool bEnhanced = CurrentEnergy >= EnergyCost;
	ChosenBodyMontage = bEnhanced ? EnhancedMontage : AttackMontage;

	if (bEnhanced)
	{
		// 手动 Apply——引擎 AbilityCosts 克隆 CDO spec, 无法携带 per-instance
		// SetByCaller 幅值。能量变化一律走 GE(聚合器 + value-change delegate 存活)。
		FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
		FGameplayEffectSpecHandle CostSpec =
			ASC->MakeOutgoingSpec(UZZZGameplayEffect_EnergyDelta::StaticClass(), 1.0f, Ctx);
		if (CostSpec.IsValid())
		{
			CostSpec.Data->SetSetByCallerMagnitude(GameplayTags.Data_Energy, -EnergyCost);
			ASC->ApplyGameplayEffectSpecToSelf(*CostSpec.Data.Get());
		}
	}

	// 2) Decision-window end (FollowUp template): 完美闪避的玩家慢放只用于决策,
	//    特殊技激活即结束——否则全程 0.5x。
	{
		FGameplayTagContainer SlowTags;
		SlowTags.AddTag(GameplayTags.State_SlowMotion);
		ASC->RemoveActiveEffectsWithGrantedTags(SlowTags);
	}

	// 3) 转向(可选)后按入口档位播放: 有起手 → 两段式(Lead 完成切主体);
	//    免起手/槽空 → 直连主体。
	if (bRotateToTarget)
	{
		RotateToTargetTask = UAbilityTask_RotateToTarget::RotateToTarget(
			this, TargetSearchRadius, RotateInterpSpeed);
		RotateToTargetTask->ReadyForActivation();
	}

	UAnimMontage* LeadMontage = ResolveLeadMontage();
	if (LeadMontage)
	{
		bLeadPending = true;
		UE_LOG(LogZZZCombatRemake, Log,
			TEXT("%s: special lead '%s' (%s) — body follows on lead completion"),
			*GetName(), *LeadMontage->GetName(),
			bEnhanced ? TEXT("enhanced pending") : TEXT("normal pending"));
		if (!PlayMontage(LeadMontage))
		{
			return;  // PlayMontage already ended the ability (null montage)
		}
	}
	else
	{
		UE_LOG(LogZZZCombatRemake, Log,
			TEXT("%s: special direct body (%s) — no lead (free/skip entry)"),
			*GetName(), bEnhanced ? TEXT("enhanced") : TEXT("normal"));
		if (!PlayMontage(ChosenBodyMontage))
		{
			return;  // PlayMontage already ended the ability (null montage)
		}
	}
}

void UZZZSpecialAttack::OnMontageCompleted()
{
	// 起手完成 → 切主体(两段式): 主体播放由基类回调链收尾(完成/打断 → EndAbility)。
	if (bLeadPending)
	{
		bLeadPending = false;
		UE_LOG(LogZZZCombatRemake, Log,
			TEXT("%s: special lead complete — playing body '%s'"),
			*GetName(), ChosenBodyMontage ? *ChosenBodyMontage->GetName() : TEXT("<null>"));
		if (!PlayMontage(ChosenBodyMontage))
		{
			return;  // PlayMontage already ended the ability (null montage)
		}
		return;
	}

	Super::OnMontageCompleted();
}

void UZZZSpecialAttack::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	bLeadPending = false;

	// Window-tag fallback (CLAUDE.md rule 1 layer A): a montage interruption
	// may skip AnimNotifyState::NotifyEnd, leaving combo-window tags stuck on
	// the ASC and wrongly routing later inputs. This ability grants no windows
	// itself — the cleanup is defensive against a mis-copied BasicAttack
	// notify layout. Removal of a not-granted tag is a no-op — safe.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		const FZZZGameplayTags& GameplayTags = FZZZGameplayTags::Get();
		ASC->RemoveLooseGameplayTag(GameplayTags.Effect_Ability_CanCombo);
		ASC->RemoveLooseGameplayTag(GameplayTags.Effect_Input_CanBuffer);
		ASC->RemoveLooseGameplayTag(GameplayTags.State_Combat_Recovery);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
