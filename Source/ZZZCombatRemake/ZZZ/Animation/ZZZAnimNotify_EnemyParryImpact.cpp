// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZAnimNotify_EnemyParryImpact.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Effects/ZZZStatusGameplayEffects.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"  // FGameplayEventData (ParryImpact broadcast)
#include "Kismet/GameplayStatics.h"
#include "Tags/ZZZGameplayTags.h"
#include "ZZZCombatRemake.h"  // LogZZZCombatRemake

UZZZAnimNotify_EnemyParryImpact::UZZZAnimNotify_EnemyParryImpact()
{
	// Default GE slots — C++ carriers (same default-in-ctor pattern as
	// HitStopEffect / SlowMotionEffect). Per-instance overridable with BP
	// children in the montage editor.
	FreezeEffect = UZZZGameplayEffect_ParryStop::StaticClass();
	StaggerEffect = UZZZGameplayEffect_Stagger::StaticClass();

	// Default cue tag — ini-registered (Config/DefaultGameplayTags.ini) so the
	// RequestGameplayTag succeeds even though this CDO is constructed during
	// module load, before native tags register (CLAUDE.md 规则 2). Runtime
	// fallback in Notify covers a cold-load miss anyway.
	ImpactCueTag = FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.ParryImpact"), false);
}

void UZZZAnimNotify_EnemyParryImpact::Notify(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (!MeshComp)
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		return;
	}

	// The notify lives on the ENEMY's attack montage — its own ASC is the
	// consumer and the freeze/stagger target.
	IAbilitySystemInterface* GAS = Cast<IAbilitySystemInterface>(Owner);
	UAbilitySystemComponent* ASC = GAS ? GAS->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return;
	}

	const FZZZGameplayTags& Tags = FZZZGameplayTags::Get();

	// Not being parried → the flag is absent → nothing to do (a normal swing
	// plays through to its damage frame as usual).
	if (!ASC->HasMatchingGameplayTag(Tags.Effect_Enemy_ParryPending))
	{
		return;
	}

	// Consume: one parry per attack. If the flag somehow survived to a LATER
	// attack's notify, the tag was already removed here — no double parry.
	ASC->RemoveLooseGameplayTag(Tags.Effect_Enemy_ParryPending);

	// ── 1. 敌人定格 (敌人定格由 notify 触发 — 设计 2026-09-04): Duration GE →
	// TimeDilation → CustomTimeDilation 桥 (AZZZCombatEnemy)。冻结从下一帧起生效,
	// 本 notify 的剩余步骤同帧继续执行。
	if (FreezeEffect)
	{
		FGameplayEffectContextHandle FreezeContext = ASC->MakeEffectContext();
		FGameplayEffectSpecHandle FreezeSpec = ASC->MakeOutgoingSpec(FreezeEffect, 1.0f, FreezeContext);
		if (FreezeSpec.IsValid())
		{
			ASC->ApplyGameplayEffectSpecToSelf(*FreezeSpec.Data.Get());
		}
	}

	// ── 2. 招架特效 (cue 无 handler 时静默 — 资产后补)。
	const FGameplayTag CueTag = ImpactCueTag.IsValid()
		? ImpactCueTag
		: Tags.GameplayCue_ZZZ_ParryImpact;
	if (CueTag.IsValid())
	{
		FGameplayCueParameters CueParams;
		CueParams.Instigator = Owner;
		ASC->ExecuteGameplayCue(CueTag, CueParams);
	}

	// ── 3. 同步冻结招架者 (人物定格由招架 GA 触发 — 同帧): 招架者是按键瞬间已 Possess
	// 的现役玩家 pawn。给其 ASC 广播 Event.Combat.ParryImpact —— 现役招架 GA 实例在
	// ActivateAbility 绑定的监听消费并自施冻结(事件路由保证调用的是实例而非 CDO,
	// 2026-09-05 ensure 修复; 事件分发同步, 与敌人自施冻结同帧)。招架 GA 已结束
	// (招架被早取消)则无监听 → 事件静默, 人物不冻, 敌人侧照常定格+打断(安全)。
	if (UWorld* World = Owner->GetWorld())
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				if (IAbilitySystemInterface* PlayerGAS = Cast<IAbilitySystemInterface>(Pawn))
				{
					if (UAbilitySystemComponent* PlayerASC = PlayerGAS->GetAbilitySystemComponent())
					{
						FGameplayEventData ParryData;
						ParryData.Instigator = Owner;
						ParryData.Target = Pawn;
						PlayerASC->HandleGameplayEvent(Tags.Event_Combat_ParryImpact, &ParryData);
					}
				}
			}
		}
	}

	// ── 4. 本次攻击被打断: CancelAbilities(Ability.Attack.Enemy) → OnInterrupted →
	// EndAbility(取消) → 兜底清理 AttackWindow / Dodged / ParryPending (双轨②)。
	// 蒙太奇以 bStopWhenAbilityEnds=false 播放 → 取消后无人认领, 但正被定格 GE 钉住,
	// 冻结结束后挥击后摇收尾自然恢复。
	FGameplayTagContainer CancelTags;
	CancelTags.AddTag(Tags.Ability_Attack_Enemy);
	ASC->CancelAbilities(&CancelTags);

	// ── 5. 普通受击硬直 (槽 2): 照 AttributeSet 受击路径 — spec + DynamicGrantedTags
	// (State.Staggered, C 层 GE 管理 — 禁止 LooseTag)。
	if (StaggerEffect)
	{
		FGameplayEffectContextHandle StaggerContext = ASC->MakeEffectContext();
		FGameplayEffectSpecHandle StaggerSpec = ASC->MakeOutgoingSpec(StaggerEffect, 1.0f, StaggerContext);
		if (StaggerSpec.IsValid())
		{
			StaggerSpec.Data->DynamicGrantedTags.AddTag(Tags.State_Staggered);
			ASC->ApplyGameplayEffectSpecToSelf(*StaggerSpec.Data.Get());
		}
	}

	UE_LOG(LogZZZCombatRemake, Log,
		TEXT("[%s] EnemyParryImpact: parried attack consumed — freeze+cancel+stagger"),
		*GetNameSafe(Owner));
}
