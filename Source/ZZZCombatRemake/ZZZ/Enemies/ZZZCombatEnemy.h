// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"  // FGameplayTag (FindNearestEnemy RequiredState)
#include "ZZZAttributeSet.h"
#include "ZZZCombatEnemy.generated.h"

class UAbilitySystemComponent;
class UZZZAttributeSet;
class UWidgetComponent;
class UZZZEnemyHeadWidget;
class AZZZCharacter;

/**
 * Simple enemy for ZZZ combat MVP.
 * Owns its ASC + AttributeSet directly (no PlayerState — this is not a player).
 * Takes damage via GAS GameplayEffect. No AI yet.
 *
 * Phase 3 Task 1: minimal tick-driven attack behavior so perfect dodge /
 * parry have something to react to. Replaced by StateTree AI in Phase 6 —
 * the logic is kept in one place (TryStartAttack) for an easy swap.
 */
UCLASS(abstract)
class AZZZCombatEnemy : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AZZZCombatEnemy();

	// === IAbilitySystemInterface ===
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** Access the enemy's attribute set */
	UZZZAttributeSet* GetAttributeSet() const { return AttributeSet; }

	// === Test helpers (PIE: number key 1 or console command ZZZ.PauseEnemies) ===
	// Pause early-outs TryStartAttack: NEW attacks and chasing are blocked,
	// but an in-flight attack finishes normally (no cancel). Test-only
	// convenience — deliberately NOT a GE-managed gameplay state.
	//
	// Global pause covers every enemy (static, single-player only); per-enemy
	// SetPaused is for future per-target testing / debug UI.

	/** Toggle the global pause over ALL enemies' attack AI. */
	static void ToggleGlobalPause();

	/** Current global pause state (debug UI / HUD display). */
	static bool IsGlobalPauseEnabled() { return bGlobalPauseEnabled; }

	/** Pause/resume this enemy's attack AI only. */
	UFUNCTION(BlueprintCallable, Category = "ZZZ|EnemyAI")
	void SetPaused(bool bInPaused);

	/** Whether this enemy's attack AI is individually paused. */
	UFUNCTION(BlueprintPure, Category = "ZZZ|EnemyAI")
	bool IsPaused() const { return bIsPaused; }

	/**
	 * Nearest living, visible enemy to Origin that carries RequiredState (empty
	 * tag = any). Filters hidden actors and State.Dead. Single implementation
	 * shared by UZZZGameplayAbility::FindNearestEnemy (dodge perfect judgment)
	 * and the PC's parry auto-judgment (2026-09-04).
	 */
	static AZZZCombatEnemy* FindNearestEnemy(
		UWorld* World, const FVector& Origin, float Radius, const FGameplayTag& RequiredState);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Called whenever Health changes — logs old/new values for MVP verification */
	void OnHealthChanged(const FOnAttributeChangeData& Data);

	/** Mirrors the TimeDilation attribute to CustomTimeDilation (Phase 3 Task 3b). */
	void OnTimeDilationChanged(const FOnAttributeChangeData& Data);

	// === Minimal attack behavior (Phase 3 test harness; Phase 6 → StateTree) ===

	/** Master switch — off by default; BP_EnemyTest_Manny enables it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZZZ|EnemyAI")
	bool bEnableAttackBehavior = false;

	/** Attack ability granted on BeginPlay (GA_EnemyAttack). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZZZ|EnemyAI",
		meta = (EditCondition = "bEnableAttackBehavior"))
	TSubclassOf<UGameplayAbility> AttackAbilityClass;

	/** Attack is started when the player is inside this range (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZZZ|EnemyAI",
		meta = (EditCondition = "bEnableAttackBehavior"))
	float AttackRange = 220.0f;

	/** Players beyond this range are ignored entirely (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZZZ|EnemyAI",
		meta = (EditCondition = "bEnableAttackBehavior"))
	float AggroRange = 1000.0f;

	/** Minimum seconds between attacks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZZZ|EnemyAI",
		meta = (EditCondition = "bEnableAttackBehavior"))
	float AttackCooldown = 2.2f;

private:
	/** Tick-driven attack check: guard tags → cooldown → nearest player → chase/face → activate. */
	void TryStartAttack(float DeltaSeconds);

	float LastAttackTime = -FLT_MAX;

	/** Global test pause — consulted by TryStartAttack on every enemy. */
	static bool bGlobalPauseEnabled;

	/** Per-enemy test pause (SetPaused). */
	bool bIsPaused = false;

protected:
	// === Per-enemy config (set per Blueprint subclass) ===

	/** Starting max health for this enemy type. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZZZ|Stats")
	float InitialMaxHealth = 100.0f;

	/** Starting max daze for this enemy type. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZZZ|Stats")
	float InitialMaxDaze = 200.0f;

	/** Base attack power (unused until enemy AI is implemented). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZZZ|Stats")
	float InitialAttack = 0.0f;

	/** Base defense power. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZZZ|Stats")
	float InitialDefense = 10.0f;

	// === 敌人头顶状态条 (2026-09-08, UI-Design §三) ===

	/**
	 * Screen-space WidgetComponent 挂点——浮于头部上方, 自动面向相机 (飘字先例)。
	 * Widget 实例由 BeginPlay 按 HeadWidgetClass 创建; 未配置 = 无 widget 不渲染,
	 * 旧 BP 无需任何改动。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ZZZ|UI")
	TObjectPtr<UWidgetComponent> HeadStatus;

	/** 头顶条 Widget 类 (WBP_EnemyHead——UZZZEnemyHeadWidget 子类), 每个敌人 BP 上填。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZZZ|UI")
	TSubclassOf<UZZZEnemyHeadWidget> HeadWidgetClass;

	/** 名字条显示名 (可留空 → WBP 自行折叠名字区)。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZZZ|UI")
	FText DisplayName;

	/**
	 * 挂点相对根高度 (cm)。根 = 胶囊中心, Mannequin 头顶约 +92cm——初值 90 让条贴头顶;
	 * 每骨架按需微调 (UI-Design §八)。BeginPlay 应用, BP 改值无需重编译。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZZZ|UI")
	float HeadBarHeight = 90.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UZZZAttributeSet> AttributeSet;
};
