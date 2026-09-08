// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "ZZZPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class AZZZCharacter;
class AZZZCombatEnemy;
class UZZZDamageNumberWidget;
class UZZZDamageNumberPool;
class UZZZPlayerHUDWidget;

/**
 * PlayerController for ZZZ combat.
 *
 * Owns the per-player input buffer — the single source of truth for
 * "what did the player last press during a buffering window".
 * Written by WaitInputBuffer, read/consumed by WaitCombo.
 */
UCLASS()
class AZZZPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AZZZPlayerController(const FObjectInitializer& ObjectInit);

	void SetBufferedInput(FGameplayTag InputTag)     { BufferedInput = InputTag; }
	FGameplayTag ConsumeBufferedInput()              { FGameplayTag T = BufferedInput; BufferedInput = FGameplayTag(); return T; }
	bool HasBufferedInput() const                    { return BufferedInput.IsValid(); }

	/** Switch to the next squad member (persistent members, hidden when inactive). */
	UFUNCTION(BlueprintCallable, Category = "ZZZ|Squad")
	void SwitchToNextCharacter();

	/**
	 * 招架支援路径 (2026-09-04) — 切换键判定命中 AttackWindow 敌人时执行:
	 * 候选成员摆到敌人正前方(朝向敌人) → BeginSwitchIn(不播 EnterMontage) →
	 * Possess → 激活其招架 GA (资产 tag Ability.Defense.Assist, 类扫描) →
	 * 激活成功才给敌人挂 Effect.Enemy.ParryPending (敌人定格帧 notify 消费)。
	 * 激活失败(BP 未配置/被阻塞) = 敌人攻击照常、新人物站场, 不挂 tag。
	 */
	void TryParrySwitch(AZZZCombatEnemy* ParryEnemy);

	/** Pooled damage number accessor (used by the GameplayCue). */
	UZZZDamageNumberPool* GetDamageNumberPool() const { return DamageNumberPool; }

	// === Squad roster accessors (HUD 只读, 2026-09-08) ===
	// UI-Design §一.5: 轮转序真源 = SquadClasses, TeamPanel 槽序与切人逻辑共用。
	// 只读不改存储形态; 槽序旋转/实例解析由 Widget 侧按类完成 (SquadMembers 按
	// 首次登场/注册序追加, 不可假设与 SquadClasses index 对齐——一律按类查找)。

	/** 队伍人数 N（roster 大小 = 面板槽数上限）。 */
	UFUNCTION(BlueprintPure, Category = "ZZZ|Squad")
	int32 GetSquadRosterSize() const { return SquadClasses.Num(); }

	/** Roster 下标 → 职业类（越界 → nullptr）。 */
	UFUNCTION(BlueprintPure, Category = "ZZZ|Squad")
	UClass* GetSquadRosterClass(int32 RosterIndex) const;

	/** 该职业的已登场实例（从未切换进场 → nullptr；隐藏成员照常返回——属性实时）。 */
	UFUNCTION(BlueprintPure, Category = "ZZZ|Squad")
	AZZZCharacter* GetSquadMemberByClass(UClass* RosterClass) const;

	/** 当前操作角色在轮转序的下标（不在 roster → INDEX_NONE）。 */
	UFUNCTION(BlueprintPure, Category = "ZZZ|Squad")
	int32 GetCurrentSquadIndex(AZZZCharacter* CurrentPawn) const;

protected:
	UPROPERTY(EditAnywhere, Category = "Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Squad roster in switch order (BP_Okuma + BP_Okuma_Switch for the prototype). */
	UPROPERTY(EditDefaultsOnly, Category = "ZZZ|Squad")
	TArray<TSubclassOf<AZZZCharacter>> SquadClasses;

	/** Input action that triggers SwitchToNextCharacter (bound on the PC so it survives pawn changes). */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Input Mappings")
	TObjectPtr<UInputAction> SwitchAction;

	// === Damage numbers ===

	/** Widget class for floating damage numbers (WBP_DamageNumber). */
	UPROPERTY(EditDefaultsOnly, Category = "ZZZ|DamageNumber")
	TSubclassOf<UZZZDamageNumberWidget> DamageNumberWidgetClass;

	/** Initial pool size (grows on demand up to a cap). */
	UPROPERTY(EditDefaultsOnly, Category = "ZZZ|DamageNumber")
	int32 DamageNumberPoolSize = 32;

	// === Squad HUD (2026-09-08, UI-Design §2) ===

	/** 屏幕 HUD 根 WBP（WBP_ZZZHUD — UZZZPlayerHUDWidget 子类），BP_ZZZPlayerController 上填。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZZZ|HUD")
	TSubclassOf<UZZZPlayerHUDWidget> HUDWidgetClass;

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	/**
	 * Gameplay Cameras (manager mode): activate the possessed character's
	 * GameplayCameraComponent here so BOTH the initial possess and every squad
	 * switch set the view target through one path (component auto-activation
	 * only builds its evaluation context — it never sets itself as view target).
	 */
	virtual void OnPossess(APawn* InPawn) override;

private:
	UPROPERTY()
	TObjectPtr<UZZZDamageNumberPool> DamageNumberPool;
	/** 屏幕 HUD 实例（CreateHUD 一次创建, 会话期复用）。 */
	UPROPERTY(Transient)
	TObjectPtr<UZZZPlayerHUDWidget> HUDWidget;
	/** Spawned squad instances (hidden while inactive) — 按首次登场/注册序追加, 勿假设与 SquadClasses index 对齐。 */
	UPROPERTY()
	TArray<TObjectPtr<AZZZCharacter>> SquadMembers;

	/** Find a live instance of CharacterClass, or spawn one and remember it. */
	AZZZCharacter* GetOrSpawnSquadMember(UClass* CharacterClass);

	/** 屏幕 HUD：CreateWidget + AddToViewport（BeginPlay 锚点——首次 Possess 可能早于 PC BeginPlay，见 RefreshHUD）。 */
	void CreateHUD();

	/** 刷新 Squad HUD（槽序/高亮/能量重绑全由 Possess 驱动）；HUD 未建或无玩家 pawn 时静默。 */
	void RefreshHUD();

	/** True if a live instance of CharacterClass currently carries State.Dead. */
	bool IsSquadClassEliminated(UClass* CharacterClass) const;

	/** 候选类选择（2026-09-04 自 SwitchToNextCharacter 抽出共享）：跳过当前职业与阵亡职业。失败 → 屏幕提示 + 返回 null。 */
	UClass* PickNextSquadClass(AZZZCharacter* CurrentPawn);

private:
	/** The last input buffered during this attack window. Empty tag = nothing buffered. */
	FGameplayTag BufferedInput;

	/** Number key 1 handler — toggles the global enemy-AI pause (PIE test helper; console: ZZZ.PauseEnemies). */
	void ToggleEnemyPause();

	// === Switch flow (2026-08-31) ===

	/** 切换进行中（旧人物未完全隐藏）——屏蔽再次切换。 */
	bool bIsSwitching = false;

	/** 进场成员相对旧人物的后退偏移（cm，沿旧人物 forward 反向；≈入场动画前冲位移，动画冲完恰好到位）。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZZZ|Squad")
	float SwitchInOffset = 2000.0f;

	/** 进场成员相对旧人物的右偏偏移（cm，沿旧人物 right 方向；右后方站位，与旧人物错开）。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZZZ|Squad")
	float SwitchInRightOffset = 250.0f;

	// === Assist auto-judgment (2026-09-04) ===

	/** 招架自动判定半径 (cm) — 切换键按下时在此半径内找最近的 AttackWindow 敌人。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZZZ|Assist")
	float ParryDetectRadius = 300.0f;

	/** 招架者入场点 = 被招架敌人正前方此距离 (cm)（≈招架姿势与打击点的贴合距离, 特效掩盖余量）。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZZZ|Assist")
	float AssistEntryDistance = 120.0f;

	/** 旧人物完全隐藏时回调——清除 bIsSwitching 守卫（AddUniqueDynamic 绑定，见 SwitchToNextCharacter）。 */
	UFUNCTION()
	void OnSwitchOutCompleted(AZZZCharacter* SwitchedOutCharacter);
};
