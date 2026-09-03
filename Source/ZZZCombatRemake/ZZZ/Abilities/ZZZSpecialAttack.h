// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ZZZGameplayAbility.h"
#include "ZZZSpecialAttack.generated.h"

class UAbilityTask_RotateToTarget;

/**
 * Special attack (特殊技) — Y key, no cooldown. One Blueprint subclass per
 * character; every gameplay difference is data on that BP.
 *
 * ── Move structure (2026-09-03 定稿) ─────────────────────────────────
 *   特殊技 = 可选起手段 (Lead, 含打击1: 弱伤害/长前摇) + 主体 (Body:
 *   打击2, 主要伤害源)。主体分普通/强化两版(强化 = Energy >= EnergyCost,
 *   判定与扣费在本 GA 内); 起手与普通/强化无关、普通/强化共用一套。
 *
 *   Entry contexts → lead tier (前驱活动 GA 的资产 tag 匹配, 见下):
 *     自由态 / 无前驱          → LeadFullMontage    (A, 完整前摇)
 *     普攻段(如 1/3)          → LeadComboMontage   (B, 短衔接)  [ComboLeadContextTags]
 *     冲刺攻击(如 dash 尾窗)   → LeadDashMontage    (C)           [DashLeadContextTags]
 *     普攻段(如 2/4)          → 无起手 → 直连主体                [DirectEntryContextTags]
 *   档位规则表(下三数组)按段位资产 tag 填 —— 2/4 段的尾帧姿态与主体首帧衔接是
 *   动画师契约; A/B/C 起手出口也必须收敛到同一姿态(主体首帧)。
 *   空槽/未匹配降级: 无前驱或前驱 tag 不匹配任何表 → A(完整); 匹配档的起手槽
 *   为空 → 直连主体。
 *
 * ── Entry resolution (GA 内自扫, 2026-09-03) ─────────────────────────
 *   Activation 是同步的: InternalTryActivateAbility 在调用栈内立即执行, 而
 *   前驱 GA 要到本技蒙太奇启动(打断前驱蒙太奇)后才 EndAbility —— 因此本 GA
 *   激活瞬间仍能扫到"前驱活动 GA"并读其资产 tag(GetAssetTags), 与角色门控
 *   无需任何上下文传递(门控只做机制判定: 窗口+自由态/忙)。
 *
 * ── BP 配置契约 (GA_SpecialAttack_*) ────────────────────────────────
 *   Asset Tags            = { Ability.Attack.Basic, Ability.Attack.Special }
 *     (Basic: 普攻起手抑制/切人等待/闪避可取消 全部顺带覆盖; Special: 定位 + 自链防护)
 *   AttackMontage         = 主体·普通版 (打击2)
 *   EnhancedMontage       = 主体·强化版 (打击2, 伤害/Daze 更高)
 *   LeadFullMontage / LeadComboMontage / LeadDashMontage = 起手 A/B/C (可空)
 *   DirectEntryContextTags / ComboLeadContextTags / DashLeadContextTags
 *                         = 前驱段位资产 tag → 档位 (如 BasicAttack02/04 / 01/03 / dash tag)
 *   EnergyCost            = 强化门槛 == 消耗 (默认 50)
 *   bRotateToTarget       = true
 *   勿配: AbilityTriggers / NextComboAbility / EndEventTag。
 *
 * ── 生命周期 ────────────────────────────────────────────────────────
 *   Lead 段: PlayMontage(Lead); 完成后由覆写的 OnMontageCompleted 切主体
 *   (bLeadPending 状态拦截基类 EndAbility), 随后按能量选 Attack/EnhancedMontage
 *   直播; 无起手入口则直接播主体。整段动作(Lead+Body)属一个 GA —— 蒙太奇
 *   内不放窗口 notify、不放 AttackEnd notify(误放 = GA 提前结束); 收尾后
 *   自由态, 攻击从普攻第 1 段重起。激活时清 State.SlowMotion(完美闪避慢放,
 *   防 0.5x 全程, FollowUp 模板); EndAbility 兜底清窗 tag(防误拷 notify)。
 */
UCLASS(Abstract)
class UZZZSpecialAttack : public UZZZGameplayAbility
{
	GENERATED_BODY()

public:
	UZZZSpecialAttack();

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

	/** Lead 完成 → 切主体（两段式播放的衔接点）。 */
	virtual void OnMontageCompleted() override;

protected:
	// === Lead-in tier montages (起手段 = 含打击1: 弱伤害/长前摇; 普通/强化共用) ===

	/** A 档: 自由态完整起手(默认档——无前驱或前驱未匹配任何规则表时使用)。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Special")
	TObjectPtr<UAnimMontage> LeadFullMontage;

	/** B 档: 普攻段短衔接起手（前驱 tag ∈ ComboLeadContextTags）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Special")
	TObjectPtr<UAnimMontage> LeadComboMontage;

	/** C 档: 冲刺攻击衔接起手（前驱 tag ∈ DashLeadContextTags）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Special")
	TObjectPtr<UAnimMontage> LeadDashMontage;

	// === Entry-context rule tables (前驱活动 GA 资产 tag → 档位) ===

	/** 前驱在这些资产 tag 上 → 免起手直连主体（快速打击, 如普攻 2/4 段）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Special",
		meta = (ToolTip = "Preceding GA asset tags that grant a LEAD-SKIPPING direct body entry (e.g. BasicAttack02/04)"))
	TArray<FGameplayTag> DirectEntryContextTags;

	/** 前驱在这些资产 tag 上 → 起手 B（普攻段短衔接）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Special",
		meta = (ToolTip = "Preceding GA asset tags that pick the Combo lead (e.g. BasicAttack01/03)"))
	TArray<FGameplayTag> ComboLeadContextTags;

	/** 前驱在这些资产 tag 上 → 起手 C（冲刺攻击衔接）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Special",
		meta = (ToolTip = "Preceding GA asset tags that pick the Dash lead (e.g. the dash-attack asset tag)"))
	TArray<FGameplayTag> DashLeadContextTags;

	// === Body montages (主体 = 仅打击2: 主要伤害源) ===

	/** 强化特殊技主体(能量 ≥ EnergyCost 时播放)。普通主体 = 基类 AttackMontage。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Special")
	TObjectPtr<UAnimMontage> EnhancedMontage;

	/** 强化门槛 == 消耗：Energy >= EnergyCost 触发强化主体并扣除 EnergyCost。默认 50。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Special",
		meta = (ClampMin = "0.0"))
	float EnergyCost = 50.0f;

	// === Targeting (same trio as UZZZFollowUpAttack) ===

	/** When enabled, the character auto-rotates toward the nearest enemy during this attack. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Targeting")
	bool bRotateToTarget = true;

	/** Sphere radius (cm) for finding the nearest enemy at ability activation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Targeting",
		meta = (EditCondition = "bRotateToTarget"))
	float TargetSearchRadius = 500.0f;

	/** Yaw interpolation speed toward target (higher = snappier, 5-15 typical). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Targeting",
		meta = (EditCondition = "bRotateToTarget"))
	float RotateInterpSpeed = 10.0f;

	UPROPERTY()
	TObjectPtr<UAbilityTask_RotateToTarget> RotateToTargetTask;

private:
	/** 解析入口档位 → 要播的起手(可空 = 直连主体)。激活时自扫前驱活动 GA 资产 tag。 */
	UAnimMontage* ResolveLeadMontage() const;

	/** 主体选择(普通/强化) + 扣费在激活时一次完成; Lead 结束后据此续播。 */
	TObjectPtr<UAnimMontage> ChosenBodyMontage;

	/** Lead 刚播完、主体待启动——覆写的 OnMontageCompleted 据此拦截基类 EndAbility。 */
	bool bLeadPending = false;
};
