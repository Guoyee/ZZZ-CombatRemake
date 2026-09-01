// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameplayCameraComponent.h"
#include "InputCoreTypes.h"
#include "ZZZCharacter.h"
#include "ZZZCombatEnemy.h"
#include "ZZZCombatRemake.h"
#include "ZZZDamageNumberPool.h"
#include "ZZZDamageNumberWidget.h"

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

	// ── 1. Pick the next candidate class ──
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
			TEXT("SwitchToNextCharacter: no valid candidate class found."));
		// 2026-08-31: 之前静默失败=用户看到"没反应"。屏幕提示阵亡原因 —
		// 只有当前角色存活时, 切换无候选 = 其他成员全部阵亡。
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(1, 3.0f, FColor::Yellow,
				TEXT("No squad member available to switch to — others are eliminated."));
		}
		return;
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
	// facing on move. SwitchInOffset 让新人物沿旧人物 forward 反向让位（避免同点
	// 重叠；0 = 与旧人物同点，保持原行为）。
	const FVector SwitchInLocation = CurrentPawn
		? CurrentPawn->GetActorLocation() - CurrentPawn->GetActorForwardVector() * SwitchInOffset
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

	// ── 7. Re-activate the new member's gameplay camera ──
	// A freshly spawned member auto-activates at BeginPlay, but a member we
	// switch back to was already activated (and lost the view target when the
	// last switch took it). Explicit re-activation re-sets the view target to
	// the new pawn, which drives CA_PlayerCameras' EnterTransitions blend —
	// the smooth camera transition on switch. Rotation stays player-controlled
	// (both characters share CR_ThirdPerson, driven by ControlRotation).
	if (UGameplayCameraComponent* CameraComp =
			NextMember->FindComponentByClass<UGameplayCameraComponent>())
	{
		CameraComp->ActivateCameraForPlayerController(this);
	}

	UE_LOG(LogZZZCombatRemake, Log,
		TEXT("SwitchToNextCharacter: switched to '%s'."), *GetNameSafe(NextClass));
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
