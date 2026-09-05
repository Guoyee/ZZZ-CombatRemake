// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "GameplayEffect.h"
#include "Engine/World.h"
#include "Effects/ZZZStatusGameplayEffects.h"
#include "Tags/ZZZGameplayTags.h"
#include "ZZZCharacter.h"  // 命中回能: instigator → 玩家角色的公开回能入口

namespace
{
	/**
	 * Applies a status GE that grants Tag at apply time via DynamicGrantedTags
	 * (CLAUDE.md rule 1 layers B/C — GE-managed tag lifecycle). All ZZZ status
	 * carriers are tag-less CDOs (module-load timing, rule 2), so the tag is
	 * always injected here.
	 */
	FActiveGameplayEffectHandle ApplyStatusEffect(
		UAbilitySystemComponent* TargetASC,
		TSubclassOf<UGameplayEffect> EffectClass,
		const FGameplayTag& Tag)
	{
		if (!TargetASC || !EffectClass)
		{
			return FActiveGameplayEffectHandle();
		}

		FGameplayEffectContextHandle Context = TargetASC->MakeEffectContext();
		FGameplayEffectSpecHandle Spec = TargetASC->MakeOutgoingSpec(EffectClass, 1.0f, Context);
		if (!Spec.IsValid())
		{
			return FActiveGameplayEffectHandle();
		}
		Spec.Data->DynamicGrantedTags.AddTag(Tag);
		return TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}

UZZZAttributeSet::UZZZAttributeSet()
{
	InitHealth(100.0f);
	InitMaxHealth(100.0f);
	InitEnergy(0.0f);
	InitMaxEnergy(100.0f);
	InitDaze(0.0f);
	InitMaxDaze(200.0f);
	InitAttack(50.0f);
	InitDefense(20.0f);
	InitIncomingDamage(0.0f);
	InitTimeDilation(1.0f);
}

void UZZZAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetEnergyAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxEnergy());
	}
	else if (Attribute == GetDazeAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxDaze());
	}
}

void UZZZAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	const FGameplayEffectContextHandle& Context = Data.EffectSpec.GetContext();
	UAbilitySystemComponent* SourceASC = Context.GetOriginalInstigatorAbilitySystemComponent();
	AActor* TargetActor = GetOwningActor();
	UAbilitySystemComponent* TargetASC = GetOwningAbilitySystemComponent();

	// ═══════════════════════════════════════════════════════════
	// IncomingDamage consumption (Meta Attribute core)
	//   ExecCalc → IncomingDamage (+FinalDamage)
	//   PostGEExecute → IncomingDamage → Health deduction → zero
	// ═══════════════════════════════════════════════════════════
	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float DamageAmount = GetIncomingDamage();
		SetIncomingDamage(0.0f);  // Consume immediately — never leave residue

		if (DamageAmount <= 0.0f || bIsDead)
		{
			return;
		}

		// ═══════════════════════════════════════════════════════════
		// Invulnerability intercept (dodge i-frames — State.Invulnerable is
		// granted by GA_Dodge's ActivationOwnedTags)
		//   While invulnerable, damage is fully absorbed. Short-circuiting here
		//   means NO health loss, NO damage number, NO Hit event, NO death check
		//   — a dodged hit shows no feedback beyond the dodge itself.
		//   (2026-08-09: the perfect-dodge JUDGMENT moved to press time inside
		//   UZZZDodge — it queries the enemy's attack window when the dodge
		//   starts. The old hit-frame CanDodge window / DodgePerfect broadcast
		//   chain was removed with that change.)
		// ═══════════════════════════════════════════════════════════
		const FZZZGameplayTags& GameplayTags = FZZZGameplayTags::Get();
		if (TargetASC && TargetASC->HasMatchingGameplayTag(GameplayTags.State_Invulnerable))
		{
			return;
		}

		// Apply damage to Health
		// (Shield / mitigation hooks go HERE — before SetHealth)
		const float NewHealth = GetHealth() - DamageAmount;
		SetHealth(FMath::Clamp(NewHealth, 0.0f, GetMaxHealth()));

		// Spawn floating damage number (GameplayCue)
		if (TargetASC)
		{
			FGameplayCueParameters CueParams;
			CueParams.Instigator = Context.GetInstigator();
			// Note: U																																														E 5.8 removed FGameplayCueParameters::TargetActor — the
			// cue manager resolves the target from the ASC's avatar itself.
			CueParams.Location = TargetActor
				? TargetActor->GetActorLocation() + FVector(0, 0, 100)
				: FVector::ZeroVector;
			CueParams.RawMagnitude = DamageAmount;
			TargetASC->ExecuteGameplayCue(
				FZZZGameplayTags::Get().GameplayCue_ZZZ_DamageNumber, CueParams);
		}

		// Broadcast Hit event
		if (SourceASC)
		{
			FGameplayEventData HitData;
			HitData.Instigator = Context.GetInstigator();
			HitData.Target = TargetActor;
			HitData.EventMagnitude = DamageAmount;
			SourceASC->HandleGameplayEvent(
				FZZZGameplayTags::Get().Event_Combat_Hit, &HitData);
		}

		// ────────────────────────────────────────────────────────
		// 命中回能 (2026-09-03): 伤害确认单点——DamageAmount>0 且已穿过无敌拦截,
		// 击杀帧也在此(死亡判定在下方)。条件: 来源是玩家角色(敌人 instigator 是
		// AZZZCombatEnemy, Cast 自然短路)且目标是敌人(防未来友伤/中立物误给能)。
		// 幅值 = 攻击者的 per-hit 配置; 变化走 GAS (源 ASC 上的能量 GE)。
		// ────────────────────────────────────────────────────────
		if (TargetASC
			&& TargetASC->HasMatchingGameplayTag(GameplayTags.State_Enemy))
		{
			if (AZZZCharacter* SourceCharacter = Cast<AZZZCharacter>(Context.GetInstigator()))
			{
				SourceCharacter->ApplyEnergyDelta(SourceCharacter->GetEnergyGainPerHit());
			}
		}

		// Death detection
		if (GetHealth() <= 0.0f)
		{
			bIsDead = true;

			if (TargetASC)
			{
				// Layer C (core state, GE-managed — no LooseTag): State.Dead is
				// granted by an Infinite GE that lives until ASC reset/destroy.
				ApplyStatusEffect(TargetASC, UZZZGameplayEffect_Dead::StaticClass(),
					FZZZGameplayTags::Get().State_Dead);
			}

			if (SourceASC)
			{
				FGameplayEventData ElimData;
				ElimData.Instigator = Context.GetInstigator();
				ElimData.Target = TargetActor;
				SourceASC->HandleGameplayEvent(
					FZZZGameplayTags::Get().Event_Combat_Elimination, &ElimData);
			}
		}

		// ═══════════════════════════════════════════════════════════
		// Enemy hit-stagger (Phase 3 Task 4)
		//   A surviving enemy hit gets State.Staggered (read by the switch
		//   auto-judgment in Task 5 — parry/assault detection) and its attack
		//   is cancelled (GA End → State.Attacking removed by
		//   ActivationOwnedTags). Layer C: State.Staggered is granted by a
		//   self-expiring Duration GE (0.35s) — no LooseTag, no removal timer,
		//   no dangling-lambda hazard if the enemy dies mid-stagger. (Hit-stop
		//   no longer lives here — the attacker's notify applies HitStopEffect.)
		// ═══════════════════════════════════════════════════════════
		if (GetHealth() > 0.0f
			&& TargetASC
			&& TargetASC->HasMatchingGameplayTag(GameplayTags.State_Enemy))
		{
			ApplyStatusEffect(TargetASC, UZZZGameplayEffect_Stagger::StaticClass(),
				GameplayTags.State_Staggered);

			FGameplayTagContainer CancelTags;
			CancelTags.AddTag(GameplayTags.Ability_Attack_Enemy);
			TargetASC->CancelAbilities(&CancelTags);
		}
	}

	// ═══════════════════════════════════════════════════════════
	// Daze / Stun detection
	// ═══════════════════════════════════════════════════════════
	if (Data.EvaluatedData.Attribute == GetDazeAttribute())
	{
		SetDaze(FMath::Clamp(GetDaze(), 0.0f, GetMaxDaze()));

		// 失衡判据 = GE 管理的 State.Stun tag (2026-09-05): _Stun 已改 Duration 自
		// 过期 (5.0s) —— tag 在场 = 失衡中 (不重复触发), 到期随 GE 移除 → 可再次失衡。
		// 替代 bIsStunned bool: bool 无法感知 GE 过期, 会导致恢复后无法再次失衡。
		if (TargetASC
			&& GetDaze() >= GetMaxDaze()
			&& !TargetASC->HasMatchingGameplayTag(FZZZGameplayTags::Get().State_Stun))
		{
			SetDaze(0.0f);  // Reset buildup

			// Layer C: State.Stun via a self-expiring Duration GE (no LooseTag).
			ApplyStatusEffect(TargetASC, UZZZGameplayEffect_Stun::StaticClass(),
				FZZZGameplayTags::Get().State_Stun);

			if (SourceASC)
			{
				FGameplayEventData StunData;
				StunData.Instigator = Context.GetInstigator();
				StunData.Target = TargetActor;
				SourceASC->HandleGameplayEvent(
					FZZZGameplayTags::Get().Event_Combat_Stun, &StunData);
			}
		}
	}
}
