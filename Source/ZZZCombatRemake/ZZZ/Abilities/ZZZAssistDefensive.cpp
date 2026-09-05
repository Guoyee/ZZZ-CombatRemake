// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZAssistDefensive.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Effects/ZZZStatusGameplayEffects.h"
#include "GameFramework/Character.h"
#include "GameplayEffect.h"
#include "Tags/ZZZGameplayTags.h"
#include "ZZZCombatRemake.h"

UZZZAssistDefensive::UZZZAssistDefensive()
{
	// Asset tags / activation owned tags are configured on the Blueprint
	// (GA_AssistDefensive) — see UZZZEnemyAttack header for the CDO timing
	// rationale. FreezeEffect defaults to the C++ carrier (no tag config in its
	// ctor — a plain Duration GE, safe at CDO time); per-GA-BP overridable.
	FreezeEffect = UZZZGameplayEffect_ParryStop::StaticClass();
}

void UZZZAssistDefensive::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bParryImpacted = false;

	// 定格事件监听 (2026-09-05): 敌人定格帧 notify 向本 ASC 广播 Event.Combat.ParryImpact
	// → 事件分发到本 GA 实例(实例方法, 绝非 CDO——修复 ensure) → OnParryImpact()。
	// 同 EndEventTag 监听纪律: 此处绑定、EndAbility 解绑。notify 在招架 GA 结束后
	// 广播 = 无监听者, 事件静默。
	if (UAbilitySystemComponent* EventASC = GetAbilitySystemComponentFromActorInfo())
	{
		const FGameplayTag ParryEventTag = FZZZGameplayTags::Get().Event_Combat_ParryImpact;
		FGameplayEventMulticastDelegate& EventDelegate =
			EventASC->GenericGameplayEventCallbacks.FindOrAdd(ParryEventTag);
		ParryImpactHandle = EventDelegate.AddUObject(this, &UZZZAssistDefensive::OnParryImpact);
	}

	// Parry montage + EndAbility wiring lives in the base class (Task 1a).
	// The parry pose starts at frame 1 (资产契约: 起手即招架动作, 无前置入场) — the
	// enemy's strike-frame notify fires 1~20 frames later (AttackWindow 起点 ≈0.059s
	// 最早按键 → ParryImpact 0.392s 最长余量; 窗口末 ≈0.375s 按键 → 约 1 帧) and calls
	// OnParryImpact() to pin us via the freeze GE. 招架姿势需 hold 覆盖该最差余量。
	if (!PlayAttackMontage())
	{
		return;  // PlayAttackMontage already ended the ability (null montage)
	}

	// 支援突击 handoff (2026-09-04/05, 直接走 CanCombo 连招逻辑): the parry
	// montage's recover tail carries the generic CanCombo window; an attack
	// input inside it hands off through the base-class combo machinery
	// (GetComboNext → NextComboAbility = GA_AssistRush, BP 配置). Spawned
	// unconditionally (same as the dodge): if the montage never opens the
	// window the task dies with this ability.
	TrySetupComboHandoff();
}

void UZZZAssistDefensive::OnParryImpact(const FGameplayEventData* Payload)
{
	// 敌人定格帧事件消费——与敌人自施冻结同帧(事件分发同步)。定格只发一次(同帧多
	// 广播/迟到通知防重复施加)。本方法跑在能力实例上(事件路由)——CurrentActorInfo
	// 有效, 见 ActivateAbility 绑定处注释。
	if (bParryImpacted)
	{
		return;
	}
	bParryImpacted = true;

	AActor* Avatar = GetAvatarActorFromActorInfo();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!Avatar || !ASC)
	{
		return;
	}

	// 两段式 + 段跳转 (2026-09-05 定稿): 先跳到 Recover 段再冻结——定格瞬间的姿势恒
	// = Recover 段首帧(资产把它做成招架定格姿势), 与按键时刻(敌人定格帧余量 1~20 帧)
	// 无关; 早按蒙太奇已过 Recover 首帧时反向跳转的 pop 被同帧冻结掩盖。段缺失/空名
	// → 保持当前帧(旧线性行为)。跳转与冻结同帧执行: 冻结从下一帧起稀释动画推进。
	if (RecoverSectionName != NAME_None && AttackMontage)
	{
		if (ACharacter* Character = Cast<ACharacter>(Avatar))
		{
			if (UAnimInstance* Anim = Character->GetMesh()->GetAnimInstance())
			{
				if (AttackMontage->GetSectionIndex(RecoverSectionName) != INDEX_NONE)
				{
					Anim->Montage_JumpToSection(RecoverSectionName, AttackMontage);
				}
				else
				{
					UE_LOG(LogZZZCombatRemake, Warning,
						TEXT("UZZZAssistDefensive: section '%s' NOT FOUND in '%s' — freezing at current frame"),
						*RecoverSectionName.ToString(), *GetNameSafe(AttackMontage));
				}
			}
		}
	}

	if (!FreezeEffect)
	{
		return;
	}

	// 人物定格: Duration GE → TimeDilation → CustomTimeDilation 桥（AZZZCharacter）,
	// 招架姿势被钉住; 到期后 aggregator 恢复（慢放/正常）。槽默认 ParryStop(0.2s/0.01),
	// 与敌人侧 notify 槽同帧开始、可各自覆写调时长。
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(FreezeEffect, 1.0f, Context);
	if (Spec.IsValid())
	{
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		UE_LOG(LogZZZCombatRemake, Log,
			TEXT("UZZZAssistDefensive: parry impact — jumped to '%s' + self freeze applied (定格)"),
			*RecoverSectionName.ToString());
	}
}

void UZZZAssistDefensive::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// 定格事件监听解绑——迟到的 ParryImpact 广播不能打进已结束的能力(同 EndEventHandle
	// 与 UZZZDodge::PlayerSlowEventHandle 的纪律)。
	if (ParryImpactHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			const FGameplayTag ParryEventTag = FZZZGameplayTags::Get().Event_Combat_ParryImpact;
			if (FGameplayEventMulticastDelegate* Delegate =
				ASC->GenericGameplayEventCallbacks.Find(ParryEventTag))
			{
				Delegate->Remove(ParryImpactHandle);
				ParryImpactHandle.Reset();
			}
		}
	}

	// Window-tag fallback (CLAUDE.md rule 1 layer A): the recover-tail window
	// (Effect.Ability.CanCombo, granted by an AbilityWindow notify on the parry
	// montage) must not survive the ability — e.g. the follow-up strike's
	// montage superseded this one before NotifyEnd ran. A stale window would
	// wrongly admit a special attack or route a later attack input into the
	// assist follow-up out of a dead parry. Removal of a not-granted tag is a
	// no-op — safe unconditionally.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->RemoveLooseGameplayTag(FZZZGameplayTags::Get().Effect_Ability_CanCombo);
	}

	// State.Invulnerable needs no manual removal — ActivationOwnedTags.
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
