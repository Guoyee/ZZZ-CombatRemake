// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ZZZGameplayAbility.h"
#include "ZZZFollowUpAttack.generated.h"

class UAbilityTask_RotateToTarget;

/**
 * Dodge-window follow-up strikes — the single-strike attacks fired by the
 * attack input while a dodge's displacement window is open.
 *
 * 2026-09-03 (追击技 = 闪避的连段段): these are NOT engine-trigger abilities
 * anymore — they are combo segments. The dodge arms the base-class combo
 * handoff (TrySetupComboHandoff): its displacement window grants the generic
 * Effect.Ability.CanCombo (AbilityWindow notify, layer A — CanDashAttack
 * retired), an attack input inside the window is consumed by the dodge's own
 * WaitCombo, and GetComboNext() (overridden by UZZZDodge) activates the
 * chosen strike. NO AbilityTriggers on these Blueprints (leave the arrays
 * empty, like basic-combo segments) — the character gate only skips the
 * basic-attack starter while the dodge window is open.
 *
 * Two Blueprint subclasses, referenced by UZZZDodge's follow-up slots
 * (GA_Dodge BP), each carrying its segment identity asset tag
 * (2026-09-03 — Ability.Attack.Dash(.Attack/.Counter) hierarchy):
 *   DashFollowUpAbility    — plain dodge   → GA_DashAttack
 *                          (AM_Attack_Rush,  Asset Tags ⊇ Ability.Attack.Dash.Attack)
 *   PerfectFollowUpAbility — perfect dodge → GA_DashCounter
 *                          (AM_Attack_Counter, Asset Tags ⊇ Ability.Attack.Dash.Counter)
 * The perfect/plain branch is decided at dodge press time (bIsPerfectDodge,
 * 判定前移 philosophy) — State.PerfectDodge no longer participates in routing.
 * The identity tags feed tag-based rule tables (e.g. UZZZSpecialAttack's
 * DashLeadContextTags) — keep them in sync with this header.
 *
 * Execution template (shared with neither combo nor dodge):
 *   CommitAbility → optional RotateToTarget → PlayAttackMontage → end on
 *   montage completion (base class callbacks). A follow-up strike is a
 *   terminal single hit; damage comes from ZZZAnimNotify_AttackTrace on the
 *   montage. Optional chain out (unchanged): NextComboAbility configured
 *   (e.g. GA_DashAttack → GA_BasicAttack_02) hands off from its own tail
 *   CanCombo window. The dodge window is removed by UZZZDodge::EndAbility as
 *   a fallback (CanCombo) — no tag cleanup needed here.
 */
UCLASS(Abstract)
class UZZZFollowUpAttack : public UZZZGameplayAbility
{
	GENERATED_BODY()

public:
	UZZZFollowUpAttack();

	// === UGameplayAbility overrides ===

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

protected:
	// === Targeting ===

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

	// === Assist-rush warp (支援突击穿敌, 2026-09-06) ===
	// 仅当激活时角色持有招架目标敌人（PC TryParrySwitch 写入）才生效——GA_AssistRush
	// 是招架连段段, 穿敌 warp 是它的专属语义; 普攻/闪避追击(同基类)无招架敌人引用,
	// 不受影响。目标 = 敌人正后方(敌位置 - 敌Forward×RushPassDistance), 旋转 = 位移方向
	// (穿敌后保持冲刺朝向)。蒙太奇上须摆 AnimNotifyState_MotionWarping(target 名与本槽
	// 一致) + AnimNotifyState_CollisionPassThrough(穿敌碰撞, WindowTag=State.PassThrough
	// 顺带停 RotateToTarget 索敌转向)。

	/** warp target 名 —— 与突击蒙太奇上 AnimNotifyState_MotionWarping 的 WarpTargetName 一致。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Warp")
	FName RushWarpTargetName = TEXT("ZZZ_RushLand");

	/** 突击落点距敌人后背的距离(cm)：落点 = 敌位置 - 敌Forward×RushPassDistance。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZZZ|Warp", meta = (ClampMin = "50.0"))
	float RushPassDistance = 200.0f;
};
