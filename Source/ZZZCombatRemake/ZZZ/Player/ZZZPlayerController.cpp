// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZPlayerController.h"

#include "AbilitySystemComponent.h"
#include "Abilities/ZZZAssistDefensive.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameplayCameraComponent.h"
#include "GameFramework/GameplayCamerasPlayerCameraManager.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneSequencePlaybackSettings.h"
#include "Tags/ZZZGameplayTags.h"
#include "TimerManager.h"
#include "ZZZCharacter.h"
#include "ZZZCombatEnemy.h"
#include "ZZZCombatRemake.h"
#include "ZZZDamageNumberPool.h"
#include "ZZZDamageNumberWidget.h"
#include "ZZZPlayerCameraManager.h"
#include "ZZZPlayerHUDWidget.h"

// PIE 测试命令 (2026-09-11): 分镜过场的手动验证入口 —— 大招 GA 未落地前先验证
// 镜头接管/归还本身。用法: ZZZ.PlayCinematic <SequenceAssetPath> [BindingTag]
static FAutoConsoleCommand ZZZPlayCinematicCommand(
	TEXT("ZZZ.PlayCinematic"),
	TEXT("Play a level sequence cinematic bound to the current pawn. "
	     "Usage: ZZZ.PlayCinematic <SequenceAssetPath> [BindingTag]"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&AZZZPlayerController::ConsolePlayCinematic));

AZZZPlayerController::AZZZPlayerController(const FObjectInitializer& ObjectInit)
	: Super(ObjectInit)
{
	// Gameplay Cameras manager (2026-09-01): a SINGLE camera system hosts every
	// squad member's evaluation context, so view-target changes on switch blend
	// through CA_PlayerCameras' EnterTransitions. Per-pawn standalone systems
	// (bRunStandaloneCameraSystem=true) cannot blend across characters — each
	// has its own evaluation stack, so switching = camera cut.
	// Custom subclass AZZZPlayerCameraManager (2026-09-04): carries the pitch
	// clamp (ViewPitchMin/Max) — the only legacy-camera property still honored
	// by this pipeline, applied to ControlRotation each PC tick (see class doc).
	PlayerCameraManagerClass = AZZZPlayerCameraManager::StaticClass();
}

void AZZZPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// Gameplay Cameras MANAGER mode: the camera system is owned by
	// AGameplayCamerasPlayerCameraManager, NOT by the per-pawn components.
	// Component auto-activation only builds an evaluation context (it never
	// sets a view target), and PC::SetViewTarget through the manager would
	// build a plain "actor-copy" context for pawns without a UCameraComponent
	// (camera stuck at the pawn's origin — observed). The correct entry point
	// is the manager's ActivateGameplayCamera: it pushes the component's
	// evaluation context onto the system's context stack (the view target
	// follows via OnContextStackChanged), which is exactly what lets
	// CA_PlayerCameras' EnterTransitions blend across squad members.
	//
	// Idempotency: only push while the context is NOT active. Once pushed, the
	// context stays on the stack for the whole session; switching back to a
	// member is handled by bAutoManageActiveCameraTarget → SetViewTarget, which
	// finds the existing context and moves it to the top of the stack (silent).
	// Re-pushing an active context makes ActivateGameplayCamera log an error.
	if (AZZZCharacter* ZZZPawn = Cast<AZZZCharacter>(InPawn))
	{
		if (UGameplayCameraComponent* CameraComp =
				ZZZPawn->FindComponentByClass<UGameplayCameraComponent>())
		{
			TSharedPtr<const UE::Cameras::FCameraEvaluationContext> Context =
				CameraComp->GetEvaluationContext();
			const bool bAlreadyPushed = Context.IsValid() && Context->IsActive();

			if (!bAlreadyPushed)
			{
				if (AGameplayCamerasPlayerCameraManager* GPCameraManager =
						Cast<AGameplayCamerasPlayerCameraManager>(PlayerCameraManager))
				{
					GPCameraManager->ActivateGameplayCamera(
						CameraComp, EGameplayCameraComponentActivationMode::Push);
				}
				else
				{
					UE_LOG(LogZZZCombatRemake, Error,
						TEXT("OnPossess: PlayerCameraManager is not a GameplayCameras manager "
						     "(class '%s') — GameplayCameraComponent will not run."),
						*GetNameSafe(PlayerCameraManager ? PlayerCameraManager->GetClass() : nullptr));
				}
			}
		}
	}

	// Squad HUD (2026-09-08, UI-Design §2.2): every possess — the initial one,
	// normal switches, and parry support — refreshes the team panel (slot
	// rotation + highlight + rebind) and the skill button (current member's
	// energy). Possess is the single signal both switch paths share, so no
	// extra broadcast is needed. The very first possess can run before the HUD
	// exists (PC BeginPlay) — CreateHUD covers that case with its own refresh.
	RefreshHUD();
}

void AZZZPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Add input mapping contexts to the Enhanced Input subsystem
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		for (UInputMappingContext* MappingContext : DefaultMappingContexts)
		{
			if (MappingContext)
			{
				Subsystem->AddMappingContext(MappingContext, 0);
			}
		}
	}

	// Pooled damage numbers (Screen-space widgets) — pre-created, no hit-time
	// spawning. Widget class comes from the PC asset (WBP_DamageNumber).
	DamageNumberPool = NewObject<UZZZDamageNumberPool>(this);
	DamageNumberPool->Initialize(GetWorld(), DamageNumberPoolSize, DamageNumberWidgetClass);

	// 预加载所有小队成员（2026-09-08）：为 roster 里每个职业各准备一个隐藏实例
	// ——设计口径是"成员只生成一次、隐藏而非销毁"，但此前开局只生成当前角色，
	// 其余槽位在首次切换前没有实例（HUD 只能显示占位）。开局全部生成 → 每个槽
	// 都有真实成员，头像/血条/能量/名字全实时，且后台回能照常（隐藏成员仍 Tick）。
	// 当前操作角色由 GameMode 出生并已 possess → 直接登记，避免重复生成。
	AZZZCharacter* InitialPawn = Cast<AZZZCharacter>(GetPawn());
	if (InitialPawn && SquadClasses.Contains(InitialPawn->GetClass()))
	{
		SquadMembers.AddUnique(InitialPawn);
	}
	for (const TSubclassOf<AZZZCharacter>& SquadClass : SquadClasses)
	{
		if (!SquadClass || (InitialPawn && SquadClass == InitialPawn->GetClass()))
		{
			continue;
		}
		if (AZZZCharacter* Member = GetOrSpawnSquadMember(SquadClass))
		{
			// 未登场成员保持隐藏 + 关碰撞（与退场后状态一致；进场由 BeginSwitchIn 恢复）。
			Member->SetActorHiddenInGame(true);
			Member->SetActorEnableCollision(false);
		}
	}

	// Squad HUD (2026-09-08): CreateWidget + AddToViewport, then refresh once —
	// the initial Possess (GameMode RestartPlayer) can run BEFORE this BeginPlay
	// (OnPossess skipped for lack of a HUD), so GetPawn() is pulled here. Every
	// later possess refreshes from OnPossess instead.
	CreateHUD();
}

void AZZZPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Switch action binds on the PC's InputComponent, which is shared with
	// (and survives changes of) the possessed pawn — so the switch key works
	// regardless of which squad member is currently active.
	// Started (not Triggered): Triggered fires every frame while held, which
	// would cycle the squad rapidly for the whole duration of the press.
	if (UEnhancedInputComponent* EI = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (SwitchAction)
		{
			EI->BindAction(SwitchAction, ETriggerEvent::Started,
				this, &AZZZPlayerController::SwitchToNextCharacter);
		}
	}

	// PIE test helper: number key 1 toggles the global enemy-AI pause. Bound
	// as a legacy key binding (no InputAction asset needed — stays a pure
	// debug convenience and works alongside Enhanced Input). Console
	// equivalent: ZZZ.PauseEnemies. (Not F5 — the editor reserves F5 for the
	// Shader Complexity view mode and steals it in PIE.)
	InputComponent->BindKey(EKeys::One, IE_Pressed,
		this, &AZZZPlayerController::ToggleEnemyPause);
}

void AZZZPlayerController::ToggleEnemyPause()
{
	AZZZCombatEnemy::ToggleGlobalPause();
}

ALevelSequenceActor* AZZZPlayerController::PlayCinematic(ULevelSequence* Sequence, AActor* BindActor, FName BindingTag)
{
	if (!Sequence)
	{
		UE_LOG(LogZZZCombatRemake, Warning, TEXT("PlayCinematic: null sequence — ignored."));
		return nullptr;
	}

	// 同一时间只允许一个过场: 先停掉上一个 (含清理可能残留的宿主 actor)。
	StopCinematic();

	FMovieSceneSequencePlaybackSettings Settings;
	Settings.bDisableMovementInput = true;  // pose 阶段为纯演出 —— 锁操作
	Settings.bDisableLookAtInput = true;
	// 相机接管由 Sequence 的 Camera Cut Track 负责 —— 不要 bDisableCameraCuts。

	ALevelSequenceActor* SeqActor = nullptr;
	CinematicPlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(GetWorld(), Sequence, Settings, SeqActor);
	if (!CinematicPlayer || !SeqActor)
	{
		UE_LOG(LogZZZCombatRemake, Error, TEXT("PlayCinematic: failed to create player for '%s'."), *GetNameSafe(Sequence));
		CinematicPlayer = nullptr;
		return nullptr;
	}
	CinematicSequenceActor = SeqActor;
	CinematicPlayer->OnFinished.AddDynamic(this, &AZZZPlayerController::HandleCinematicFinished);

	// 动态绑定 (大招在任意位置释放 → Sequence 里的角色占位在运行时指向当前操作角色)。
	// 相机机位在 Sequence 里以"相对绑定角色"编排时, 这一绑就是全部对位工作。
	if (BindActor && !BindingTag.IsNone())
	{
		SeqActor->SetBindingByTag(BindingTag, TArray<AActor*>{ BindActor }, /*bAllowBindingsFromAsset*/ false);
	}

	CinematicPlayer->Play();
	UE_LOG(LogZZZCombatRemake, Log, TEXT("PlayCinematic: '%s' started (binding '%s' → '%s')."),
		*GetNameSafe(Sequence), *BindingTag.ToString(), *GetNameSafe(BindActor));
	return SeqActor;
}

void AZZZPlayerController::StopCinematic()
{
	if (CinematicPlayer)
	{
		// Stop 触发各 section 的结束处理 —— CameraCut section 配 Restore State 时
		// 在此归还 view target (SetViewTarget(原 Pawn) → 组件 context 移顶 → 主 rig 回来)。
		CinematicPlayer->Stop();
		CinematicPlayer = nullptr;
	}
	if (CinematicSequenceActor)
	{
		CinematicSequenceActor->Destroy();
		CinematicSequenceActor = nullptr;
	}
}

void AZZZPlayerController::HandleCinematicFinished()
{
	// 自然播完: 相机归还在 section 的结束处理里已完成 (Restore State → 主 rig)。
	// 宿主 actor 的销毁推迟到下一帧 —— OnFinished 在 Sequencer 求值回调栈里广播,
	// 当场销毁 player 的宿主会踩求值后置处理。
	CinematicPlayer = nullptr;
	if (ALevelSequenceActor* SeqActor = CinematicSequenceActor)
	{
		CinematicSequenceActor = nullptr;
		if (UWorld* World = GetWorld())
		{
			TWeakObjectPtr<ALevelSequenceActor> WeakActor(SeqActor);
			World->GetTimerManager().SetTimerForNextTick([WeakActor]()
			{
				if (ALevelSequenceActor* Actor = WeakActor.Get())
				{
					Actor->Destroy();
				}
			});
		}
	}
}

void AZZZPlayerController::ConsolePlayCinematic(const TArray<FString>& Args)
{
	// FAutoConsoleCommand 是静态注册的, 无 this —— 找 PIE/Game world 的第一个玩家 PC。
	UWorld* World = nullptr;
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if ((Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game) && Context.World())
			{
				World = Context.World();
				break;
			}
		}
	}
	AZZZPlayerController* PC = World ? Cast<AZZZPlayerController>(World->GetFirstPlayerController()) : nullptr;
	if (!PC)
	{
		UE_LOG(LogZZZCombatRemake, Warning, TEXT("ZZZ.PlayCinematic: no AZZZPlayerController — run in PIE/Game."));
		return;
	}

	if (Args.Num() < 1 || Args[0].IsEmpty())
	{
		UE_LOG(LogZZZCombatRemake, Warning,
			TEXT("ZZZ.PlayCinematic: usage — ZZZ.PlayCinematic <SequenceAssetPath> [BindingTag]"));
		return;
	}

	ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *Args[0]);
	if (!Sequence)
	{
		UE_LOG(LogZZZCombatRemake, Warning, TEXT("ZZZ.PlayCinematic: failed to load '%s'."), *Args[0]);
		return;
	}

	const FName BindingTag = Args.Num() > 1 ? FName(*Args[1]) : NAME_None;
	PC->PlayCinematic(Sequence, PC->GetPawn(), BindingTag);
}

void AZZZPlayerController::SwitchToNextCharacter()
{
	if (SquadClasses.Num() == 0)
	{
		UE_LOG(LogZZZCombatRemake, Warning,
			TEXT("SwitchToNextCharacter: SquadClasses is empty (configure on the PC asset)."));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(1, 3.0f, FColor::Red,
				TEXT("Switch failed: SquadClasses not configured on the PlayerController."));
		}
		return;
	}

	// 切换进行中（旧人物未完全隐藏）——屏蔽再次切换
	if (bIsSwitching)
	{
		UE_LOG(LogZZZCombatRemake, Verbose,
			TEXT("SwitchToNextCharacter: switch already in progress — ignored."));
		return;
	}

	AZZZCharacter* CurrentPawn = Cast<AZZZCharacter>(GetPawn());

	// ── 0. 招架自动判定 (2026-09-04, 状态优先级语义): 切换键按下时, 最近的
	// AttackWindow 敌人(黄闪前摇段 — 与极限闪避共用窗口)命中 → 招架支援路径:
	// 新人物在敌人正前方入场并激活招架 GA; 敌人攻击自然挥至定格帧, 由
	// ParryImpact notify 触发定格 + 打断。否则回落普通切换。
	if (CurrentPawn)
	{
		if (AZZZCombatEnemy* ParryEnemy = AZZZCombatEnemy::FindNearestEnemy(
			GetWorld(), CurrentPawn->GetActorLocation(), ParryDetectRadius,
			FZZZGameplayTags::Get().Effect_Enemy_AttackWindow))
		{
			TryParrySwitch(ParryEnemy);
			return;
		}
	}

	// ── 1. Pick the next candidate class ── (shared with TryParrySwitch)
	UClass* NextClass = PickNextSquadClass(CurrentPawn);
	if (!NextClass)
	{
		return;  // PickNextSquadClass already surfaced the reason on screen
	}

	// ── 2. Register the outgoing pawn (e.g. the GameMode-spawned starter) ──
	// so it can be re-shown on a later switch back.
	if (CurrentPawn)
	{
		SquadMembers.AddUnique(CurrentPawn);
	}

	// ── 3. Get or spawn the next member and bring it onto the field ──
	// Persistent model: members are spawned once and HIDDEN while inactive
	// (never destroyed) — required for switch-in/switch-out animations,
	// assist/chain-attack call-ins, and smooth high-frequency switching.
	AZZZCharacter* NextMember = GetOrSpawnSquadMember(NextClass);
	if (!NextMember)
	{
		UE_LOG(LogZZZCombatRemake, Error,
			TEXT("SwitchToNextCharacter: failed to spawn '%s'."), *GetNameSafe(NextClass));
		return;
	}

	// ── 3. Align position AND facing with the outgoing pawn while hidden ──
	// The incoming member inherits the outgoing one's orientation so the
	// camera (which follows ControlRotation, not the actor) and the switch-in
	// read as one continuous character. bOrientRotationToMovement re-takes
	// facing on move.
	// 入场起点 = 旧人物右后方 (2026-09-01)：后退 SwitchInOffset（≈入场动画前冲位移，
	// 动画冲完恰好到位）+ 右偏 SwitchInRightOffset（与旧人物错开站位）。新人物朝向
	// 继承旧人物，前冲动画沿旧人物 facing 方向冲出。
	const FVector SwitchInLocation = CurrentPawn
		? CurrentPawn->GetActorLocation()
			- CurrentPawn->GetActorForwardVector() * SwitchInOffset
			+ CurrentPawn->GetActorRightVector() * SwitchInRightOffset
		: GetSpawnLocation();
	NextMember->SetActorLocationAndRotation(
		SwitchInLocation,
		CurrentPawn ? CurrentPawn->GetActorRotation() : FRotator::ZeroRotator);

	// ── 4. Enter immediately — no waiting for the outgoing pawn (2026-08-31) ──
	// 切换键按下新人物立即进场（复位透明度 → 显示 → 开碰撞 → 进场动画），新旧两人
	// 短暂同场；旧人物由 StartSwitchOut 异步退场。守卫先置位——旧人物完全隐藏前
	// 屏蔽再次切换。
	bIsSwitching = true;
	NextMember->BeginSwitchIn();

	// ── 5. Drop stale buffered input BEFORE StartSwitchOut ──
	// Per-pawn ASC means the outgoing pawn's combat state (abilities, window
	// tags, cooldowns) is naturally isolated — nothing to clean there. Only
	// the shared PC input buffer must not leak into the new pawn. (旧 WaitCombo
	// 的关窗 flush 会在 StartSwitchOut 清窗口 tag 时触发，其任务级守卫
	// IsSwitchingOut 已拦下缓冲消费——此处清的是本次切换前的残留。)
	ConsumeBufferedInput();

	// ── 6. Outgoing pawn: async switch-out state machine (2026-08-31) ──
	// 等攻击 GA EndAbility（GA 结束，非蒙太奇结束）→ 退场动画 → 材质淡出 → 隐藏
	// → OnSwitchOutCompleted → bIsSwitching 放行。委托必须先在 StartSwitchOut 之前
	// 绑定：同步降级路径（无退场动画/无淡出参数）会在其调用栈内直接广播。
	if (CurrentPawn)
	{
		CurrentPawn->OnSwitchOutCompleted.AddUniqueDynamic(
			this, &AZZZPlayerController::OnSwitchOutCompleted);
		CurrentPawn->StartSwitchOut();
	}
	else
	{
		bIsSwitching = false;  // 无旧 pawn：进场即完成，守卫立即放行
	}

	// ── 6. Possess ──
	// Pure control switch: the pawn's ASC was initialized at BeginPlay
	// (InitAbilityActorInfo(this, this)) and needs no rebinding on possess.
	// NOTE: APlayerController::Possess resets the control rotation to the new
	// pawn's actor rotation — that would yank the camera to the actor's facing
	// (movement direction) and drop the player's pitch. Save and restore it so
	// the camera stays with the player-controlled view across the switch.
	const FRotator ControlRotationBeforePossess = GetControlRotation();
	Possess(NextMember);
	SetControlRotation(ControlRotationBeforePossess);

	// ── 7. Camera: handled by OnPossess — the camera view target is set
	// (ActivateCameraForPlayerController) from OnPossess, which Possess()
	// calls above. EnterTransitions on CA_PlayerCameras then blends the camera
	// position from the previous character to this one; rotation stays
	// player-controlled (CR_ThirdPerson is driven by ControlRotation).

	UE_LOG(LogZZZCombatRemake, Log,
		TEXT("SwitchToNextCharacter: switched to '%s'."), *GetNameSafe(NextClass));
}

UClass* AZZZPlayerController::PickNextSquadClass(AZZZCharacter* CurrentPawn)
{
	// Skip the current class; while the current pawn is alive, also skip
	// classes whose live instance is already eliminated (State.Dead).
	int32 CurrentIndex = INDEX_NONE;
	if (CurrentPawn)
	{
		for (int32 i = 0; i < SquadClasses.Num(); ++i)
		{
			if (SquadClasses[i] == CurrentPawn->GetClass())
			{
				CurrentIndex = i;
				break;
			}
		}
	}

	UClass* NextClass = nullptr;
	for (int32 Offset = 1; Offset <= SquadClasses.Num(); ++Offset)
	{
		const int32 Index = (CurrentIndex + Offset) % SquadClasses.Num();
		UClass* Candidate = SquadClasses[Index];
		if (!Candidate || (CurrentPawn && Candidate == CurrentPawn->GetClass()))
		{
			continue;
		}
		if ((!CurrentPawn || !CurrentPawn->IsEliminated()) && IsSquadClassEliminated(Candidate))
		{
			continue;
		}
		NextClass = Candidate;
		break;
	}

	if (!NextClass)
	{
		UE_LOG(LogZZZCombatRemake, Warning,
			TEXT("PickNextSquadClass: no valid candidate class found."));
		// 2026-08-31: 之前静默失败=用户看到"没反应"。屏幕提示阵亡原因 —
		// 只有当前角色存活时, 切换无候选 = 其他成员全部阵亡。
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(1, 3.0f, FColor::Yellow,
				TEXT("No squad member available to switch to — others are eliminated."));
		}
	}
	return NextClass;
}

void AZZZPlayerController::TryParrySwitch(AZZZCombatEnemy* ParryEnemy)
{
	if (!ParryEnemy || !ParryEnemy->GetAbilitySystemComponent())
	{
		return;
	}

	AZZZCharacter* CurrentPawn = Cast<AZZZCharacter>(GetPawn());

	// ── 1. Pick the next candidate class ──
	UClass* NextClass = PickNextSquadClass(CurrentPawn);
	if (!NextClass)
	{
		return;  // PickNextSquadClass already surfaced the reason on screen
	}

	// ── 2. Register the outgoing pawn (e.g. the GameMode-spawned starter) ──
	if (CurrentPawn)
	{
		SquadMembers.AddUnique(CurrentPawn);
	}

	// ── 3. Get or spawn the next member ──
	AZZZCharacter* NextMember = GetOrSpawnSquadMember(NextClass);
	if (!NextMember)
	{
		UE_LOG(LogZZZCombatRemake, Error,
			TEXT("TryParrySwitch: failed to spawn '%s'."), *GetNameSafe(NextClass));
		return;
	}

	// ── 4. 入场点 = 敌人正前方 (2026-09-05 符号修正) ──
	// 沿敌人面向方向(+Forward —— 敌人正前方 = 玩家所在侧)移动 AssistEntryDistance;
	// 不可从敌人位置直接减坐标/世界轴偏移。敌人只设 yaw(见 AZZZCombatEnemy AI),
	// GetActorForwardVector 即其面向; 新人物朝向 = 敌人, 仅 yaw(防跨高度俯仰倾斜)。
	const FVector EnemyLocation = ParryEnemy->GetActorLocation();
	const FVector EnemyForward = ParryEnemy->GetActorForwardVector();
	const FVector EntryLocation = EnemyLocation + EnemyForward * AssistEntryDistance;
	FRotator FaceEnemyRotation = (EnemyLocation - EntryLocation).Rotation();
	FaceEnemyRotation.Pitch = 0.0f;
	FaceEnemyRotation.Roll = 0.0f;
	NextMember->SetActorLocationAndRotation(EntryLocation, FaceEnemyRotation);

	// ── 4.5. 招架目标敌人写入新人物 (2026-09-06) ──
	// 生命周期见 AZZZCharacter::ParryEnemy 注释：招架 GA 激活时读它布落点 warp target，
	// 支援突击 GA 激活时取走并清空，招架 GA EndAbility 兜底清空。
	NextMember->SetParryEnemy(ParryEnemy);

	// ── 5. Enter immediately (同普通切换时序; 不播 EnterMontage ──
	// 入场演出 = 招架 GA 的招架蒙太奇, 激活在步骤 7) ──
	bIsSwitching = true;
	NextMember->BeginSwitchIn(/*bPlayEnterMontage=*/false);

	// ── 6. Drop stale buffered input before StartSwitchOut ──
	ConsumeBufferedInput();

	// ── 7. Outgoing pawn: normal switch-out state machine ──
	if (CurrentPawn)
	{
		CurrentPawn->OnSwitchOutCompleted.AddUniqueDynamic(
			this, &AZZZPlayerController::OnSwitchOutCompleted);
		CurrentPawn->StartSwitchOut();
	}
	else
	{
		bIsSwitching = false;
	}

	// ── 8. Possess (camera handled by OnPossess; rotation preserved) ──
	const FRotator ControlRotationBeforePossess = GetControlRotation();
	Possess(NextMember);
	SetControlRotation(ControlRotationBeforePossess);

	// ── 9. 激活招架 GA + 挂 ParryPending (2026-09-04) ──
	// 类扫描同 TryActivateSpecialAttack (资产 tag 枚举不可靠——tag 是 BP 数据):
	// 找 GA_AssistDefensive (UZZZAssistDefensive 子类, Asset Tags 含
	// Ability.Defense.Assist) 并激活。激活成功才给敌人挂 Effect.Enemy.ParryPending
	// —— 激活失败(BP 未配置/Commit 不过/tag 阻塞) = 敌人攻击照常挥出, 新人物站场,
	// 不产生定格。
	UAbilitySystemComponent* NextASC = NextMember->GetAbilitySystemComponent();
	const FZZZGameplayTags& GameplayTags = FZZZGameplayTags::Get();
	bool bParryActivated = false;
	if (NextASC)
	{
		for (const FGameplayAbilitySpec& Spec : NextASC->GetActivatableAbilities())
		{
			if (Spec.Ability
				&& Spec.Ability->GetClass()->IsChildOf(UZZZAssistDefensive::StaticClass())
				&& Spec.Ability->GetAssetTags().HasTag(GameplayTags.Ability_Defense_Assist))
			{
				if (NextASC->TryActivateAbility(Spec.Handle, /*bAllowRemoteActivation=*/true))
				{
					bParryActivated = true;
				}
				break;
			}
		}
	}

	if (bParryActivated)
	{
		ParryEnemy->GetAbilitySystemComponent()->AddLooseGameplayTag(
			GameplayTags.Effect_Enemy_ParryPending);
		UE_LOG(LogZZZCombatRemake, Log,
			TEXT("TryParrySwitch: '%s' parrying '%s' — ParryPending granted (strike frame notify pays the impact)."),
			*GetNameSafe(NextMember), *GetNameSafe(ParryEnemy));
	}
	else
	{
		UE_LOG(LogZZZCombatRemake, Warning,
			TEXT("TryParrySwitch: '%s' has no activatable GA_AssistDefensive (Ability.Defense.Assist) — enemy attack plays through."),
			*GetNameSafe(NextMember));
	}
}

void AZZZPlayerController::OnSwitchOutCompleted(AZZZCharacter* SwitchedOutCharacter)
{
	// 旧人物已完全隐藏——放行下一次切换
	bIsSwitching = false;
	UE_LOG(LogZZZCombatRemake, Log,
		TEXT("SwitchToNextCharacter: outgoing '%s' fully hidden — switch guard released."),
		*GetNameSafe(SwitchedOutCharacter));
}

AZZZCharacter* AZZZPlayerController::GetOrSpawnSquadMember(UClass* CharacterClass)
{
	if (!CharacterClass || !GetWorld())
	{
		return nullptr;
	}

	// Reuse a live (possibly hidden) instance if one was already spawned.
	for (AZZZCharacter* Member : SquadMembers)
	{
		if (Member && Member->GetClass() == CharacterClass)
		{
			return Member;
		}
	}

	// First time: spawn and remember it.
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AZZZCharacter* Member = GetWorld()->SpawnActor<AZZZCharacter>(
		CharacterClass, GetSpawnLocation(), FRotator::ZeroRotator, SpawnParams);
	if (Member)
	{
		SquadMembers.Add(Member);
	}
	return Member;
}

bool AZZZPlayerController::IsSquadClassEliminated(UClass* CharacterClass) const
{
	if (!CharacterClass || !GetWorld())
	{
		return false;
	}

	// Persistent members stay in the world while hidden, so the scan covers
	// both active and inactive squad members (spawned instances are also in
	// SquadMembers — scanning here keeps the check independent of them).
	for (TActorIterator<AZZZCharacter> It(GetWorld()); It; ++It)
	{
		AZZZCharacter* SquadCharacter = *It;
		if (SquadCharacter && SquadCharacter->GetClass() == CharacterClass && SquadCharacter->IsEliminated())
		{
			return true;
		}
	}
	return false;
}

UClass* AZZZPlayerController::GetSquadRosterClass(int32 RosterIndex) const
{
	return SquadClasses.IsValidIndex(RosterIndex) ? SquadClasses[RosterIndex] : nullptr;
}

AZZZCharacter* AZZZPlayerController::GetSquadMemberByClass(UClass* RosterClass) const
{
	if (!RosterClass)
	{
		return nullptr;
	}

	// SquadMembers 按“首次登场/注册”序追加——不假设与 SquadClasses index 对齐,
	// 一律按类查找（UI-Design §一.5 只约定轮转序真源, 不约束存储形态）。
	for (const TObjectPtr<AZZZCharacter>& Member : SquadMembers)
	{
		if (Member && Member->GetClass() == RosterClass)
		{
			return Member;
		}
	}

	// 兜底（2026-09-08）：GameMode 出生的起始成员在首次切换前**尚未注册进
	// SquadMembers**（注册发生在 SwitchToNextCharacter / TryParrySwitch）——
	// 开局 HUD 刷新时全部槽位会拿到 null（头像/血条/能量条全空）。世界扫描
	// 一次补上，口径同 IsSquadClassEliminated。
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AZZZCharacter> It(World); It; ++It)
		{
			AZZZCharacter* Candidate = *It;
			if (Candidate && Candidate->GetClass() == RosterClass)
			{
				return Candidate;
			}
		}
	}
	return nullptr;
}

int32 AZZZPlayerController::GetCurrentSquadIndex(AZZZCharacter* CurrentPawn) const
{
	if (!CurrentPawn)
	{
		return INDEX_NONE;
	}

	for (int32 i = 0; i < SquadClasses.Num(); ++i)
	{
		if (SquadClasses[i] == CurrentPawn->GetClass())
		{
			return i;
		}
	}
	return INDEX_NONE;
}

void AZZZPlayerController::CreateHUD()
{
	if (HUDWidget)
	{
		return;
	}

	if (!HUDWidgetClass)
	{
		UE_LOG(LogZZZCombatRemake, Warning,
			TEXT("CreateHUD: HUDWidgetClass not configured (set WBP_ZZZHUD on the PC asset)."));
		return;
	}

	HUDWidget = CreateWidget<UZZZPlayerHUDWidget>(this, HUDWidgetClass);
	if (!HUDWidget)
	{
		UE_LOG(LogZZZCombatRemake, Error,
			TEXT("CreateHUD: failed to create '%s'."), *GetNameSafe(HUDWidgetClass));
		return;
	}

	HUDWidget->AddToViewport();
	RefreshHUD();
}

void AZZZPlayerController::RefreshHUD()
{
	if (!HUDWidget)
	{
		return;
	}

	// 无玩家 pawn（PIE 启动空档/编辑器调试态）→ 跳过, 下一次 Possess 再刷。
	if (AZZZCharacter* CurrentPawn = Cast<AZZZCharacter>(GetPawn()))
	{
		HUDWidget->RefreshSquad(this, CurrentPawn);
	}
}
