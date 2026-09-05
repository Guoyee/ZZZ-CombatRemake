// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"  // FGameplayTag (ImpactCueTag)
#include "ZZZAnimNotify_EnemyParryImpact.generated.h"

class UGameplayEffect;

/**
 * Parry impact notify (招架定格帧, 2026-09-04).
 *
 * Placed on the ENEMY's attack montage at the strike frame (the 定格帧 — the
 * pose the enemy is pinned in during the parry freeze). Asset contract on
 * AM_EnemyAttack: AttackWindow notify (yellow flash / parry+dodge window)
 * ~10 frames before this notify; this notify before (or on) the damage-frame
 * AttackTrace — a parried strike never reaches its own trace.
 *
 * When it fires it checks whether this attack is being parried
 * (Effect.Enemy.ParryPending — granted on the enemy by the PC at the switch
 * press). If absent → no-op: a normal swing the player simply avoided, or an
 * attack nobody parried — it plays through to its damage frame as usual.
 * If present it CONSUMES the flag and runs the whole parry impact
 * synchronously (the player pressed the switch 1~20 frames ago — window
 * 0.059–0.375s vs this notify at 0.392s on AM_EnemyAttack — so the timing
 * reads as instant):
 *   1. FreezeEffect on ourselves (槽 1)   — 敌人定格, notify 触发
 *   2. ImpactCueTag on ourselves         — 招架特效 (无 handler 时静默)
 *   3. 同步冻结招架者                      — 给现役玩家 pawn 的 ASC 广播
 *      Event.Combat.ParryImpact, 由现役招架 GA 实例(激活时绑定的监听)消费自施冻结
 *      (人物定格由招架 GA 触发, 与敌人同帧; 事件路由而非 spec 直调——2026-09-05
 *      ensure 修复: spec.Ability 是 CDO, 直调实例方法会打空 CurrentActorInfo)
 *   4. CancelAbilities(Ability.Attack.Enemy) — 本次攻击被打断; 攻击 GA EndAbility
 *      兜底清理 AttackWindow / Dodged / ParryPending (layer A 双轨②)
 *   5. StaggerEffect on ourselves (槽 2)  — 普通受击硬直 (DynamicGrantedTags
 *      State.Staggered, 照 AttributeSet 受击路径)
 *
 * The freeze's TimeDilation pin holds the enemy at the strike frame through
 * the whole 定格; after it expires the cancelled attack GA's montage (played
 * with bStopWhenAbilityEnds=false) finishes its follow-through unowned and
 * the enemy recovers.
 *
 * Ownership / lifecycle (layer A loose-tag, double-track):
 *   - Grant:    AZZZPlayerController parry judgment (press time).
 *   - Consume:  this notify (one shot per attack — the tag is removed here).
 *   - Fallback: UZZZEnemyAttack::EndAbility removes the tag — an attack that
 *     ends before this notify must not leave a stale flag that a LATER
 *     attack's notify would consume.
 *
 * FreezeEffect / StaggerEffect default to the C++ carriers
 * (UZZZGameplayEffect_ParryStop / UZZZGameplayEffect_Stagger); override per
 * notify instance with BP children to retune either side.
 */
UCLASS()
class UZZZAnimNotify_EnemyParryImpact : public UAnimNotify
{
	GENERATED_BODY()

public:
	UZZZAnimNotify_EnemyParryImpact();

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	/** 敌人定格 GE（Duration GE 覆写 TimeDilation ≈0.01; 默认 ParryStop, 可覆写 BP）。 */
	UPROPERTY(EditAnywhere, Category = "Parry Impact")
	TSubclassOf<UGameplayEffect> FreezeEffect;

	/** 打断后施加的普通受击硬直 GE（默认 UZZZGameplayEffect_Stagger, 可覆写 BP）。 */
	UPROPERTY(EditAnywhere, Category = "Parry Impact")
	TSubclassOf<UGameplayEffect> StaggerEffect;

	/**
	 * 招架特效 cue tag（默认 GameplayCue.ZZZ.ParryImpact; 资产后补, 无 handler 静默）。
	 * 空 → 运行时回落单例注册 tag。
	 */
	UPROPERTY(EditAnywhere, Category = "Parry Impact", meta = (Categories = "GameplayCue"))
	FGameplayTag ImpactCueTag;
};
