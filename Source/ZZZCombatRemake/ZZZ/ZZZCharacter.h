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

	// === Switch configuration (2026-08-31, 全部可空降级) ===

	/** 切换退场需等其结束的 GA 资产 tag 列表；空 → 运行时默认 Ability.Attack.Basic（资产 tag 层级匹配 GA_01..04）。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZZZ|Switch")
	TArray<FGameplayTag> SwitchWaitAbilityTags;

	/** 退场动画；空 → 跳过动画直接材质淡出。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZZZ|Switch")
	TObjectPtr<UAnimMontage> ExitMontage;

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
	void OnExitMontageEnded(UAnimMontage* Montage, bool bInterrupted);
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
