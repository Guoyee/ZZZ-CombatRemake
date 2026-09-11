// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "ZZZPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class ALevelSequenceActor;
class AZZZCharacter;
class AZZZCombatEnemy;
class UCameraRigAsset;
class ULevelSequence;
class ULevelSequencePlayer;
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

	// === 特写相机请求 (招架特写 / 后续连携, 2026-09-11) ===
	//
	// 机制: 特写 = Main 层 rig 切换, 由 director (CDE_PlayerCamera) 每帧读取本
	// 请求决定 push 哪个 rig —— 非空 push 特写 rig, 空 push 主 rig
	// (CR_ThirdPerson)。归还 = director 下一帧改 push 主 rig, 特写按自己的
	// ExitTransitions blend out + pop。
	//
	// 为什么走 director 而不是 C++ 直接 push rig (2026-09-11 引擎源码核实):
	// ① 引擎无 "rig 自动归还" —— ExitTransitions 只是 blend 资产, 触发条件只有
	//    "被新 push 顶掉" 或显式 deactivate (PersistentBlendStackCameraNode.cpp
	//    FindExitTransition 只在移除/冻结时查找);
	// ② director 每帧调 ActivateCameraRig (TransientBlendStackCameraNode::Push
	//    去重只对 "栈顶同 context 同 rig" 生效) —— 任何侧路 push 的特写都会被
	//    director 下一帧顶掉, 所以特写必须作为 director 的决策结果 push。
	// 另: manager 模式下组件的 ActivatePersistent*CameraRig 全路径不可用
	// (EnsureCameraSystemHostIfNeeded 在 bRunStandaloneCameraSystem=false 时直接
	//  return false → HasCameraSystem()==false), 不要往那条路上走。

	/** 请求特写相机 (表现点调用, 如招架定格帧); null = 等价 ClearCloseupCamera。 */
	UFUNCTION(BlueprintCallable, Category = "ZZZ|Camera")
	void RequestCloseupCamera(UCameraRigAsset* CloseupRig);

	/** 清除特写请求 —— director 下一帧归还主 rig。 */
	UFUNCTION(BlueprintCallable, Category = "ZZZ|Camera")
	void ClearCloseupCamera();

	/** 当前特写请求 (director BP 每帧读取; null = 无特写)。 */
	UFUNCTION(BlueprintPure, Category = "ZZZ|Camera")
	UCameraRigAsset* GetRequestedCloseupRig() const { return RequestedCloseupRig; }

	// === 分镜过场 (大招 pose 阶段等, 2026-09-11) ===
	//
	// LevelSequence 全包式过场: 相机 (CameraCut) + 角色动画 + 后处理/遮挡。
	// 相机接管走 Sequencer 标准路径: CameraCut → PC->SetViewTarget(CineCameraActor)
	// → manager 建 FActorCameraEvaluationContext 压栈 (对 CineCameraActor 是正确
	// 行为, 复制其相机属性); 归还 = CameraCut section 的 When Finished =
	// Restore State → SetViewTarget(原角色 Pawn) → 栈内组件 context 移顶
	// (2026-09-11 引擎源码核实: 组件的 context owner 是组件本身, GetTypedOuter
	// 命中 Pawn → FindContextByPredicate 走"既有 context 移顶"分支, 不踩
	// actor-copy 陷阱)。⚠ 5.8 的 FMovieSceneSequencePlaybackSettings 无
	// bRestoreState 字段 (旧资料误导) —— 归还是 per-section 设置, 资产侧配。

	/**
	 * 播放分镜过场 (动态绑定 BindActor 到 Sequence 里打了 BindingTag 的 binding)。
	 * 调用前会先停掉上一个过场; 播完 (或 StopCinematic) 时宿主 actor 销毁。
	 * @param Sequence   LevelSequence 资产 (须含 Camera Cut Track)。
	 * @param BindActor  绑定对象 (通常 = 当前操作角色), 可空 (纯相机分镜)。
	 * @param BindingTag Sequence 内角色 binding 的 Tag (空 = 不绑定)。
	 * @return 宿主 actor; 失败返回 null。
	 */
	UFUNCTION(BlueprintCallable, Category = "ZZZ|Camera")
	ALevelSequenceActor* PlayCinematic(ULevelSequence* Sequence, AActor* BindActor, FName BindingTag);

	/** 停止当前过场 (Stop + 销毁宿主 actor; 未播放时为 no-op)。相机的归还由 CameraCut section 配置决定。 */
	UFUNCTION(BlueprintCallable, Category = "ZZZ|Camera")
	void StopCinematic();

	/** 当前是否在播过场。 */
	UFUNCTION(BlueprintPure, Category = "ZZZ|Camera")
	bool IsCinematicPlaying() const { return CinematicPlayer != nullptr; }

	/**
	 * PIE 测试入口 (控制台命令 ZZZ.PlayCinematic, 见 cpp 的 FAutoConsoleCommand):
	 * 加载 SequencePath 并播放, 绑定当前 Pawn。
	 *   ZZZ.PlayCinematic /Game/ZZZ/Camera/LS_Test_Koleda UltimatePlayer
	 */
	static void ConsolePlayCinematic(const TArray<FString>& Args);

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
	/** 当前特写 rig 请求（Transient；CDE_PlayerCamera 每帧读, 见 GetRequestedCloseupRig）。 */
	UPROPERTY(Transient)
	TObjectPtr<UCameraRigAsset> RequestedCloseupRig;

	/** 过场播放器（Transient; PlayCinematic 创建, 播完/停止清空）。 */
	UPROPERTY(Transient)
	TObjectPtr<ULevelSequencePlayer> CinematicPlayer;

	/** 过场宿主 actor（Transient; 随播放创建, 播完/停止销毁）。 */
	UPROPERTY(Transient)
	TObjectPtr<ALevelSequenceActor> CinematicSequenceActor;

	/** Sequence 自然播完回调（清引用; 宿主 actor 销毁推迟到下一帧）。 */
	UFUNCTION()
	void HandleCinematicFinished();
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
