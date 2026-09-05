// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"  // FActiveGameplayEffectHandle (freeze GE handle)
#include "ZZZGameplayAbility.h"
#include "ZZZAssistDefensive.generated.h"

class UGameplayEffect;

/**
 * Parry assist (招架支援, 2026-09-04) — the incoming character's parry
 * performance, activated BY THE INPUT at the switch press (no dependency on
 * the enemy's animation notify — see the class comment on
 * UZZZAnimNotify_EnemyParryImpact for the two-side split).
 *
 * Flow (input-driven, all timings owned by the enemy's strike):
 *   1. Press Space inside an enemy's AttackWindow (yellow flash) → the PC
 *      positions this character in front of the enemy and activates this GA
 *      (by asset tag Ability.Defense.Assist — no AbilityTriggers).
 *   2. ActivateAbility: Commit → PlayAttackMontage. ART CONTRACT — TWO-SECTION
 *      montage (2026-09-05 定稿 — 两段式 + 段跳转, 定格位置由段边界确定):
 *        [Guard 架势段] 任意长度(覆盖敌人最差剩余前摇——AM_EnemyAttack layout:
 *      window 0.059–0.375s → impact notify 0.392s ⇒ 按键后 1~20 帧不等)
 *        [Recover 收势段] 首帧 = 招架定格姿势(出口姿态收敛于架势末姿态);
 *      尾段挂 CanCombo 窗。敌人的定格帧 notify (UZZZAnimNotify_EnemyParryImpact)
 *      在按键后 1~20 帧触发。
 *   3. OnParryImpact() — consumes the enemy notify's Event.Combat.ParryImpact
 *      broadcast on our ASC (bound at activation, same frame as the enemy
 *      freezes itself; event routing = instance method, never the CDO):
 *      Montage_JumpToSection(RecoverSectionName) + FreezeEffect on ourselves —
 *      定格瞬间姿势恒 = Recover 段首帧, 与按键时刻无关; 敌人被钉在打击帧,
 *      两侧同帧冻结成 tableau。
 *   4. Freeze 结束 → 收势段继续 → 尾段 CanCombo 窗内按攻击 → 支援突击
 *      (GA_AssistRush via the generic combo chain, NextComboAbility —
 *      直接走 CanCombo 连招逻辑, 2026-09-05). No input → montage completes →
 *      EndAbility → idle.
 *
 * Configure on the Blueprint (GA_AssistDefensive, per character):
 *   - Asset Tags           = Ability.Defense.Assist  (identity for the PC's
 *                            class-scan activation — mirrors TryActivateSpecialAttack)
 *   - Activation Owned Tags = State.Invulnerable     (whole parry — the enemy's
 *                            strike trace / stray hits cannot hurt this character)
 *   - AttackMontage         = AM_AssistDefensive (two sections: Guard + Recover)
 *   - RecoverSectionName    = the Recover section name (default "Recover")
 *   - FreezeEffect          = 定格 GE (default C++ carrier ParryStop, override with a BP)
 *   - NextComboAbility      = GA_AssistRush (支援突击 via the generic combo chain;
 *                            empty = no follow-up, attacks in the window are terminal)
 * Asset contract for the montage — two-window default (2026-09-05 原则:
 * 技能默认支持 inputbuffer + combo, 基类 TrySetupComboHandoff 统一武装两任务):
 *   [Recover 段] 先摆 InputWindow notify state (WindowTag = Effect.Input.CanBuffer,
 *   死区), 再摆 AbilityWindow notify state (WindowTag = Effect.Ability.CanCombo).
 *   冻结期蒙太奇以 0.01 爬行 —— CanBuffer 起点贴近段跳转点(0.201)后, 定格期间按键
 *   即写入 PC 缓冲, CanCombo 开窗(解冻后)立即消费 → 支援突击支持定格期预按(连打
 *   不用二次按键)。
 * NO EndEvent/AttackEnd notify inside (same rule as the special-attack
 * montages — a stray end notify would cut the parry short).
 */
UCLASS(Abstract)
class UZZZAssistDefensive : public UZZZGameplayAbility
{
	GENERATED_BODY()

public:
	UZZZAssistDefensive();

	// === UGameplayAbility overrides ===

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

	/**
	 * 敌人定格帧事件消费（Event.Combat.ParryImpact，敌人 notify 广播到本 ASC ——
	 * 事件路由保证方法跑在能力实例上，绝非 CDO——2026-09-05 修复）：
	 * ① Montage_JumpToSection(RecoverSectionName)（两段式——定格姿势 = 收势段首帧,
	 *    与按键时刻无关; 段缺失/空名则保持当前帧）② 对自身施加 FreezeEffect——
	 *    人物定格由招架 GA 触发, 敌人定格由 notify 触发。
	 * 招架 GA 未激活（招架被早取消/GA 已结束）时无监听 → 事件静默, 无操作。
	 */
	void OnParryImpact(const FGameplayEventData* Payload);

protected:
	// === Parry freeze (定格, 2026-09-04/05) ===

	/** 定格 GE —— 施加给招架者自己（TimeDilation Duration GE）。默认 C++ 载体 ParryStop，可覆写 BP。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Assist|Parry")
	TSubclassOf<UGameplayEffect> FreezeEffect;

	/**
	 * 招架蒙太奇收势段名 (2026-09-05 定稿 — 两段式 + 段跳转): 定格瞬间把 B 蒙太奇
	 * 跳到该段再冻结——定格姿势恒 = Recover 段首帧(资产把它做成招架姿势/出口姿态),
	 * 与按键时刻(敌人定格帧余量 1~20 帧)无关。空 = 不跳转、冻在当前帧(旧线性行为)。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Assist|Parry")
	FName RecoverSectionName = TEXT("Recover");

	// === Follow-up strike (支援突击 — 招架的连段段, 2026-09-05) ===
	// 支援突击直接走通用 CanCombo 连招逻辑: 无专属追击槽/无 GetComboNext 覆写——
	// 收势尾窗按攻击 → 组合交接经基类 GetComboNext()(= NextComboAbility, 类型已放宽为
	// UGameplayAbility 族) 激活, 同普攻连段/闪避追击机制。GA BP 配 NextComboAbility
	// = GA_AssistRush; 空 = 无追击、窗内攻击按 terminal-hit 语义吞掉。

private:
	/** 本次招架是否已收到定格（防同帧重复广播重复施加冻结 GE）。 */
	bool bParryImpacted = false;

	/** Event.Combat.ParryImpact 监听句柄（ActivateAbility 绑定, EndAbility 解绑——同 EndEventHandle 纪律）。 */
	FDelegateHandle ParryImpactHandle;
};
