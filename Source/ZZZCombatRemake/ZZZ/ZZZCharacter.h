#pragma once

#include "AbilitySystemInterface.h"
#include "Engine/EngineTypes.h"  // FTimerHandle
#include "GameFramework/Character.h"
#include "GameplayEffectTypes.h"  // FOnAttributeChangeData
#include "GameplayTagContainer.h"
#include "Delegates/IDelegateInstance.h"  // FDelegateHandle (5.8: Misc/DelegateHandle.h 已并入)
#include "ZZZCharacter.generated.h"

class UAttributeSet;
struct FInputActionValue;
class UZZZAttributeSet;
class UZZZInputConfig;
class UGameplayAbility;
class UInputAction;
class UCameraComponent;
class USpringArmComponent;
class UAnimMontage;
class UMaterialInstanceDynamic;
class AZZZCharacter;  // 供下方切换委托自引用

/** 旧人物退场完成（已隐藏）时广播——PC 绑定以清除 bIsSwitching 守卫。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZZZSwitchOutCompletedSignature, AZZZCharacter*, Character);

UCLASS(abstract)
class AZZZCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	/** Constructor */
	AZZZCharacter();

	/** True when this character's ASC carries the State.Dead tag. */
	bool IsEliminated() const;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** Mirrors the TimeDilation attribute to CustomTimeDilation (same as AZZZCombatEnemy). */
	void OnTimeDilationChanged(const FOnAttributeChangeData& Data);

	// === Energy (特殊技资源, 2026-09-03) ===

	/** 每命中一名有效敌人固定回能值——被 target 侧 AttributeSet 的伤害确认处读取。 */
	float GetEnergyGainPerHit() const { return EnergyGainPerHit; }

	/**
	 * 施加带符号能量增量（走 UZZZGameplayEffect_EnergyDelta, SetByCaller
	 * Data.Energy——幅值必须先于 Apply 设置）。public：命中回能挂点（敌人侧
	 * AttributeSet 拿到的 instigator）与自然回能定时器都调它。
	 */
	void ApplyEnergyDelta(float Delta);

	// === Switch-in / switch-out (2026-08-31) ===

	/**
	 * PC 在完成新成员进场后调用（旧成员即将被 UnPossess）：进入退场状态机——
	 * 任一 SwitchWaitAbilityTags 对应 GA 活动则等其 EndAbility（含取消/打断），
	 * 然后播退场动画（可空）→ 材质淡出（FadeParameterName）→ 隐藏 → 广播 OnSwitchOutCompleted。
	 */
	void StartSwitchOut();

	/** PC 调用：复位材质透明度 → 显示 → 开碰撞 → 播进场动画（可空）。 */
	void BeginSwitchIn();

	/** 退场状态中为 true——UZZZBasicAttack / WaitCombo 据此抑制旧角色的连段与缓冲消费。 */
	bool IsSwitchingOut() const { return bSwitchingOut; }

	/** 旧人物完成隐藏时广播（PC 绑定以清除 bIsSwitching 守卫）。 */
	FZZZSwitchOutCompletedSignature OnSwitchOutCompleted;

protected:
	/**
	 * Pawn-ASC (2026-08): each squad member owns its own ASC + AttributeSet,
	 * giving naturally isolated HP / resource / cooldown tags. Same pattern as
	 * AZZZCombatEnemy. Init happens in BeginPlay — no possession dependency.
	 */
	virtual void BeginPlay() override;

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	// === Tag-based input routing (replaces old AttackPressed) ===

	/**
	 * Forwarded from Enhanced Input → ASC GameplayEvent + TryActivateAbilitiesByTag.
	 * Called for every InputAction configured in InputConfig->AbilityInputActions.
	 */
	void Input_AbilityInputTagPressed(FGameplayTag InputTag);

	/** Forwarded from Enhanced Input → ASC NotifyAbilityInputReleased (for charged attacks, etc.). */
	void Input_AbilityInputTagReleased(FGameplayTag InputTag);

	/** True while any ability carrying Tag (asset tags, e.g. Ability.Attack.Basic) has an active instance. */
	bool IsAbilityActiveWithTag(const FGameplayTag& Tag) const;

protected:
	// === Enhanced Input Actions ===

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	// === Gameplay Ability System ===

	/** Lyra-style InputAction → GameplayTag mapping DataAsset. Drives SetupPlayerInputComponent. */
	UPROPERTY(EditDefaultsOnly, Category="GAS|Input")
	TObjectPtr<UZZZInputConfig> InputConfig;

	/**
	 * Abilities granted on possession. Add GA_BasicAttack_01..04 and GA_Dodge
	 * here — dodge's activation gate lives on the ability itself
	 * (AbilityTriggers=Input.Dodge + ActivationBlockedTags), not in this class.
	 */
	UPROPERTY(EditDefaultsOnly, Category="GAS")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

	// === Stats (2026-08-31: 玩家 HP 配置化 — 之前固定 100 被敌人秒杀) ===

	/** 进场成员的血量起点(= 最大血量)。BP 可覆盖;InitAbilitySystem 时应用一次,切换不重置。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZZZ|Stats")
	float InitialMaxHealth = 1000.0f;

	// === Energy stats (2026-09-03, 特殊技资源) ===

	/** 能量上限。InitAbilitySystem 应用一次（切换不重置，与 HP 同语义）。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZZZ|Stats")
	float MaxEnergy = 100.0f;

	/** 出生能量（默认 0；PIE 调参可临时抬高以直接验证强化特殊技，验证后还原）。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZZZ|Stats")
	float InitialEnergy = 0.0f;

	/** 每命中一名有效敌人回能固定值（多目标逐目标累加；击杀帧给、被无敌吸收帧不给）。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZZZ|Stats")
	float EnergyGainPerHit = 2.0f;

	/** 自然回能速率（/秒，世界时间——不受 HitStop/慢放拉伸；隐藏中的队员照常回）。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZZZ|Stats")
	float EnergyRegenPerSecond = 1.0f;

	/** 自然回能定时器 tick 间隔（秒）——单 tick 增量 = RegenPerSecond × Interval。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZZZ|Stats", meta = (ClampMin = "0.05"))
	float EnergyRegenInterval = 0.2f;

	// === Special-attack entry config (2026-09-03) ===

	/**
	 * 在这些普攻段位的连段窗口内按 Y 触发特殊技「快速派生」（跳打击 1, 直接从
	 * QuickStrike section 起手）。空 = 永无快速派生。默认 {2,4}（Koleda 4 段）;
	 * Jane（5 段）后续按 kit 覆写。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "ZZZ|Combat")
	TArray<int32> QuickEntryComboIndexes = { 2, 4 };

	// === Switch configuration (2026-08-31, 全部可空降级) ===

	/** 切换退场需等其结束的 GA 资产 tag 列表；空 → 运行时默认 Ability.Attack.Basic（资产 tag 层级匹配 GA_01..04）。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZZZ|Switch")
	TArray<FGameplayTag> SwitchWaitAbilityTags;

	/** 退场动画（idle / GA 衔接等非移动状态）；空 → 跳过动画直接材质淡出。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZZZ|Switch")
	TObjectPtr<UAnimMontage> ExitMontage;

	/** 跑步状态退场动画（移动中切换时优先于 ExitMontage；空 → 回落 ExitMontage）。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZZZ|Switch")
	TObjectPtr<UAnimMontage> RunningExitMontage;

	/** 进场动画；空 → 直接显示。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZZZ|Switch")
	TObjectPtr<UAnimMontage> EnterMontage;

	/** 淡出驱动的材质标量参数名；材质无此参数时 SetScalarParameterValue 是安全 no-op（淡出无视觉效果，时序照常）。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZZZ|Switch")
	FName FadeParameterName = TEXT("Opacity");

	/** 材质淡出时长（秒）。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZZZ|Switch", meta = (ClampMin = "0.01"))
	float FadeDuration = 0.2f;

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> ASC;

	UPROPERTY()
	TObjectPtr<UZZZAttributeSet> AttributeSet;

private:
	void InitAbilitySystem();
	void AddCharacterAbilities();

	// === Energy regen (2026-09-03) ===

	/** 自然回能：InitAbilitySystem 末尾启动的世界 FTimer 循环（隐藏队员照常回）。 */
	void StartEnergyRegen();
	void TickEnergyRegen();
	FTimerHandle EnergyRegenTimerHandle;

	// === Special-attack input gate (2026-09-03) ===

	/**
	 * Y 键合法性 + 激活（Input.Special 分支）：
	 *   拒绝 = 切换退场中 / 已阵亡 / 特殊技自身活动中 /（忙 且 无窗口）。
	 *   忙 = 类扫描（活动 GA 是 UZZZGameplayAbility 子类）——资产 tag 枚举不可靠
	 *     （GA_DashAttack 等 tag 是 BP 数据）。
	 *   窗口 = CanCombo / CanDashAttack / State.Combat.Recovery 任一在身。
	 *   快速派生 = 忙(普攻段)且窗口在身且活动普攻 ComboIndex ∈ QuickEntryComboIndexes
	 *     → EventData 带 Event.Combat.SpecialQuickEntry 后 TryActivateAbility。
	 */
	void TryActivateSpecialAttack();

	// === Switch-out state machine ===

	enum class ESwitchOutPhase : uint8
	{
		None,
		WaitingAbilityEnd,   // 等攻击 GA EndAbility
		ExitMontage,         // 播放退场动画
		Fading,              // 材质淡出中
		Done
	};

	void TryBeginExitSequence();
	void ClearCombatWindowTags();
	void UnbindSwitchOutListeners();
	void OnWaitAbilityEnded();
	void OnDeadTagChanged(FGameplayTag Tag, int32 NewCount);
	void StartMaterialFade();
	void TickMaterialFade();
	void FinalizeSwitchOut();
	void ResetOpacityForSwitchIn();

	// === Switch invulnerability (2026-08-31) ===

	/**
	 * 切换进出场窗口的临时无敌 (State.Invulnerable, GE 管理):
	 * 退场方 StartSwitchOut → FinalizeSwitchOut;进场方 BeginSwitchIn + 1s 定时移除。
	 * Handle 精确移除,不误伤完美闪避的 GE/GA tag(共用 State.Invulnerable)。
	 */
	void ApplySwitchInvulnerability();
	void ClearSwitchInvulnerability();

	bool bSwitchingOut = false;
	ESwitchOutPhase SwitchOutPhase = ESwitchOutPhase::None;

	/** 本次切换是否等待过攻击 GA 结束 (2026-09-01)：true → 退场用 GA 衔接动画 ExitMontage；false → 跑步退场动画 RunningExitMontage。 */
	bool bSwitchWaitedForAbility = false;

	/** 材质淡出用的动态材质实例缓存（每槽一个，退场时懒创建，进场时复位）。 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> FadeMaterialInstances;

	FTimerHandle FadeTimerHandle;
	float FadeOpacity = 1.0f;

	FDelegateHandle WaitAbilityEndHandle;  // ASC->OnAbilityEnded（等 GA 结束）
	FDelegateHandle DeadTagHandle;         // ASC State.Dead tag 监听（等待期死亡安全阀）

	/** 切换无敌 GE 的 ActiveHandle（Apply 一次, 只移除自己）。 */
	FActiveGameplayEffectHandle SwitchInvulnHandle;

	/** 进场无敌的定时移除句柄（1s 窗口）。 */
	FTimerHandle SwitchInvulnTimerHandle;
};
