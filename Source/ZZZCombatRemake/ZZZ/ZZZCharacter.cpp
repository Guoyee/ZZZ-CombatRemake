#include "ZZZCharacter.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Attributes/ZZZAttributeSet.h"
#include "Components/CapsuleComponent.h"
#include "Effects/ZZZFactionGameplayEffects.h"
#include "Effects/ZZZStatusGameplayEffects.h"
#include "EnhancedInputComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Tags/ZZZGameplayTags.h"
#include "TimerManager.h"
#include "InputActionValue.h"
#include "ZZZCombatRemake.h"
#include "ZZZInputConfig.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"

AZZZCharacter::AZZZCharacter()
{
	// Pawn-ASC: the ASC must tick to progress Duration GEs (cooldown tags,
	// background DoT) while a squad member is hidden — components only tick
	// when their owning actor ticks.
	PrimaryActorTick.bCanEverTick = true;

	// Pawn-ASC (2026-08): each squad member owns its own ASC + AttributeSet.
	// Independent HP / resource / cooldown tags per character, naturally
	// isolated. Same pattern as AZZZCombatEnemy (Owner == Avatar == this).
	ASC = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("ASC"));
	AttributeSet = CreateDefaultSubobject<UZZZAttributeSet>(TEXT("AttributeSet"));
	
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// 专属玩家胶囊通道 (2026-08-31): 角色本体从 ECC_Pawn 迁到独立对象通道。
	// ZZZPlayer Profile — Block 敌人(EnemyCapsule)与世界(地板/墙, 通道默认),
	// Ignore 其他玩家(切换人物时新旧成员同时在场, 互不卡位), 显式 Ignore
	// ECC_Pawn(旧 Pawn 扫描全部落空 — 攻击检测已改阵营感知, 见
	// ZZZAnimNotify_AttackTrace)。Profile 定义在 Config/DefaultEngine.ini。
	GetCapsuleComponent()->SetCollisionProfileName(TEXT("ZZZPlayer"));

	// 镜头穿透玩家 (2026-09-01): 胶囊与 mesh 对 ECC_Camera 显式 Ignore —
	// CollisionPush 的 Camera trace 只命中世界几何(墙/地板), 不再被玩家本体
	// 或自身 mesh 遮挡。Profile 未列 Camera 时走通道默认(Block), 故需显式覆盖。
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
}

void AZZZCharacter::BeginPlay()
{
	Super::BeginPlay();
	InitAbilitySystem();
}

void AZZZCharacter::InitAbilitySystem()
{
	if (!ASC || !AttributeSet)
	{
		return;
	}

	// Single-player: Owner == Avatar == this. No possession dependency, so
	// hidden squad members are fully initialized from the moment they spawn.
	ASC->InitAbilityActorInfo(this, this);

	// 玩家血量初始化 (2026-08-31): 之前 AttributeSet 固定 100, 被敌人几刀秒杀。
	// 与敌人同款模式 — per-BP 配置 InitialMaxHealth, InitAbilitySystem 时应用一次,
	// 切换不重置(持久成员保留战损)。
	if (AttributeSet)
	{
		AttributeSet->SetMaxHealth(InitialMaxHealth);
		AttributeSet->SetHealth(InitialMaxHealth);
	}

	// Faction mark: State.Player — granted by an Infinite GE (GE-managed tag
	// lifecycle instead of a manually-managed LooseTag: cleaned up on ASC
	// reset, replicates, visible in showdebug). Used by slow-motion GE
	// exemption and targeting filters. Tag is added to the spec at apply time
	// (UZZZGameplayEffect_Faction carries no CDO tags — see its header).
	if (!ASC->HasMatchingGameplayTag(FZZZGameplayTags::Get().State_Player))
	{
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(
			UZZZGameplayEffect_Faction::StaticClass(), 1.0f, Context);
		if (Spec.IsValid())
		{
			Spec.Data->DynamicGrantedTags.AddTag(FZZZGameplayTags::Get().State_Player);
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}

	// TimeDilation bridge (2026-08-09, same pattern as AZZZCombatEnemy): the
	// player's own slow on a perfect dodge (GE_PlayerSlowMotion) modifies the
	// TimeDilation attribute via a Duration GE — PostGameplayEffectExecute does
	// NOT fire for duration effects (design doc 10.4 / M1), so mirror attribute
	// changes to CustomTimeDilation through the value-change delegate. Both the
	// GE apply (0.5) and its expiry (1.0) land here.
	ASC->GetGameplayAttributeValueChangeDelegate(
		UZZZAttributeSet::GetTimeDilationAttribute()).AddUObject(
			this, &AZZZCharacter::OnTimeDilationChanged);

	AddCharacterAbilities();
}

void AZZZCharacter::OnTimeDilationChanged(const FOnAttributeChangeData& Data)
{
	// IsNearlyEqual guard — avoid needless system updates on float jitter.
	if (FMath::IsNearlyEqual(CustomTimeDilation, Data.NewValue))
	{
		return;
	}

	// 5.8 API fact: AActor::SetCustomTimeDilation was removed — the public
	// property is the only entry point (same note as in ZZZCombatEnemy).
	CustomTimeDilation = Data.NewValue;
}

void AZZZCharacter::AddCharacterAbilities()
{
	if (!ASC) return;

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
	{
		// Dedupe: persistent members get re-possessed on switch-back; the
		// grant must not accumulate specs across (re)initializations.
		if (AbilityClass && !ASC->FindAbilitySpecFromClass(AbilityClass))
		{
			ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass));
		}
	}
}

UAbilitySystemComponent* AZZZCharacter::GetAbilitySystemComponent() const
{
	return ASC;
}

bool AZZZCharacter::IsEliminated() const
{
	return ASC && ASC->HasMatchingGameplayTag(FZZZGameplayTags::Get().State_Dead);
}

// ────────────────────────────────────────────────────────────
// Tag-based input routing
// ────────────────────────────────────────────────────────────

void AZZZCharacter::Input_AbilityInputTagPressed(FGameplayTag InputTag)
{
	if (!ASC) return;

	const FZZZGameplayTags& GameplayTags = FZZZGameplayTags::Get();
	FGameplayEventData EventData;
	EventData.Instigator = this;

	// Dash-attack / dodge-counter routing (2026-08-09): while a dodge's
	// displacement window is open (Effect.Ability.CanDashAttack — a notify
	// window tag, layer A), the attack input belongs to GA_DashAttack /
	// GA_DodgeCounter — their AbilityTriggers (Input.Attack) + Required /
	// Blocked tags route it (dodge-counter needs State.PerfectDodge, dash-
	// attack is blocked by it).
	//
	// The gate MUST be evaluated BEFORE HandleGameplayEvent (2026-08-11
	// regression fix): activating the dash attack starts its montage, which
	// interrupts the dodge montage → the dodge ability ends and
	// UZZZDodge::EndAbility removes the CanDashAttack window tag (fallback
	// cleanup). Checked afterwards, the gate sees a dead window, the
	// basic-attack starter fires on top of the dash attack, and its montage
	// supersedes the dash attack's — the player saw GA_BasicAttack_01 instead
	// of the dash attack.
	if (InputTag == GameplayTags.Input_Attack
		&& IsAbilityActiveWithTag(GameplayTags.Ability_Defense_Dodge)
		&& ASC->HasMatchingGameplayTag(GameplayTags.Effect_Ability_CanDashAttack))
	{
		ASC->HandleGameplayEvent(InputTag, &EventData);
		return;
	}

	// Broadcast for Tasks (WaitInputBuffer, etc.)
	ASC->HandleGameplayEvent(InputTag, &EventData);

	// Start GA_01 only for the attack input and only if no basic attack is
	// already running. During a combo, WaitCombo owns the transition via
	// CheckComboTransition. (Tag gate added: other input tags — e.g.
	// Input.Switch.Next — must NOT trigger the attack ability.)
	if (InputTag == GameplayTags.Input_Attack)
	{
		// Start GA_01 only if no basic attack is already running. During a
		// combo, WaitCombo owns the transition via CheckComboTransition.
		if (DefaultAbilities.Num() > 0
			&& !IsAbilityActiveWithTag(GameplayTags.Ability_Attack_Basic))
		{
			ASC->TryActivateAbilityByClass(DefaultAbilities[0]);
		}
		return;
	}

	// Other inputs (e.g. Input.Dodge) are handled by TryActivateAbilitiesByTag
	// below — abilities declare their own triggers (AbilityTriggers) and gates
	// (ActivationBlockedTags) instead of hardcoding them here.
}

bool AZZZCharacter::IsAbilityActiveWithTag(const FGameplayTag& Tag) const
{
	if (!ASC) return false;

	// GA_* carry asset tags like "Ability.Attack.Basic.BasicAttack01", which are
	// hierarchical children of the queried parent tag.
	// (GetAssetTags() — AbilityTags is deprecated in UE 5.8.)
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.Ability
			&& Spec.IsActive()
			&& Spec.Ability->GetAssetTags().HasTag(Tag))
		{
			return true;
		}
	}
	return false;
}

void AZZZCharacter::Input_AbilityInputTagReleased(FGameplayTag InputTag)
{
	if (!ASC) return;

	// TODO: Implement tag-based input release for charged attacks.
	// UE 5.8 does not have a tag-based NotifyAbilityInputReleased().
	// Options:
	//   A) Broadcast InputTag as a GameplayEvent (like Pressed does)
	//   B) Iterate activatable abilities and call AbilitySpecInputReleased(Spec)
	//   C) Use Lyra-style UAbilityInputCache subsystem
	//
	// For the MVP (no charged attacks), this is intentionally left as a stub.
	// The Enhanced Input Completed binding still fires — it just doesn't
	// route to ASC yet. Implement when Phase 3 (charged/special attacks) starts.
}

// ────────────────────────────────────────────────────────────
// Movement & Look
// ────────────────────────────────────────────────────────────

void AZZZCharacter::Move(const FInputActionValue& Value)
{
	// Recovery (backswing / dodge stand-up) windows are consumable: movement
	// input immediately ends them.
	//   1. Cancel an in-progress basic attack — 旧方式残留 (2026-08-29 定稿：
	//      新技能不依赖 Cancel，GA 由 StopAnimMontage → OnInterrupted →
	//      EndAbility 覆盖；现有 basic attack 依赖此路径，暂保留不改)。
	//   2. Stop a playing montage outright (dodge back-section transition):
	//      its ability has already ended, so only the montage remains.
	//   3. Remove the consumed window tag (CLAUDE.md 规则 1 A 层双轨兜底② —
	//      unowned section): the window can open on a section whose GA already
	//      ended (dodge stand-up: GA ends at DodgeEnd, window plays unowned),
	//      so EndAbility fallback cannot run; stopping the montage may skip
	//      NotifyEnd, so the consumer cleans up. Removal of a not-granted tag
	//      is a no-op — safe unconditionally.
	if (ASC && ASC->HasMatchingGameplayTag(FZZZGameplayTags::Get().State_Combat_Recovery))
	{
		FGameplayTagContainer CancelTags;
		CancelTags.AddTag(FZZZGameplayTags::Get().Ability_Attack_Basic);
		ASC->CancelAbilities(&CancelTags);

		StopAnimMontage();

		ASC->RemoveLooseGameplayTag(FZZZGameplayTags::Get().State_Combat_Recovery);
	}

	FVector2D MovementVector = Value.Get<FVector2D>();
	DoMove(MovementVector.X, MovementVector.Y);
}

void AZZZCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AZZZCharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AZZZCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

// ────────────────────────────────────────────────────────────
// Input setup
// ────────────────────────────────────────────────────────────

void AZZZCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Movement & Look (unchanged)
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AZZZCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AZZZCharacter::Look);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AZZZCharacter::Look);

		// Data-driven ability input bindings (replaces old hardcoded AttackPressed).
		// One-shot actions (Dodge/Switch) bind Started; hold actions (Attack) bind
		// Triggered — per FZZZInputAction::bTriggerOnStarted.
		if (InputConfig)
		{
			for (const FZZZInputAction& Action : InputConfig->AbilityInputActions)
			{
				if (Action.InputAction && Action.InputTag.IsValid())
				{
					const ETriggerEvent PressEvent = Action.bTriggerOnStarted
						? ETriggerEvent::Started
						: ETriggerEvent::Triggered;
					EnhancedInputComponent->BindAction(Action.InputAction, PressEvent,
						this, &AZZZCharacter::Input_AbilityInputTagPressed, Action.InputTag);
					EnhancedInputComponent->BindAction(Action.InputAction, ETriggerEvent::Completed,
						this, &AZZZCharacter::Input_AbilityInputTagReleased, Action.InputTag);
				}
			}
		}
	}
	else
	{
		UE_LOG(LogZZZCombatRemake, Error,
			TEXT("'%s' Failed to find an Enhanced Input component! "
			     "This template is built to use the Enhanced Input system."),
			*GetNameSafe(this));
	}
}

// ────────────────────────────────────────────────────────────
// Switch-in / switch-out (2026-08-31)
// 切换重构：新人物按键即进场（BeginSwitchIn），旧人物异步退场——
// 等攻击 GA EndAbility（非蒙太奇结束）→ 退场动画 → 材质淡出 → 隐藏 → 广播。
// ────────────────────────────────────────────────────────────

void AZZZCharacter::StartSwitchOut()
{
	if (bSwitchingOut)
	{
		return;
	}

	bSwitchingOut = true;
	SwitchOutPhase = ESwitchOutPhase::None;
	bSwitchWaitedForAbility = false;

	// 退场全程临时无敌 (2026-08-31): 从按下切换键起旧人物还站在场上(等 GA 结束/
	// 退场动画/淡出, 碰撞全开), 会被敌人攻击打死 — 之前 Jane 就是这么死的。
	// 无敌挂到 FinalizeSwitchOut(隐藏+关碰撞)为止, 之后打不到, 无需无敌。
	ApplySwitchInvulnerability();

	// 清旧角色残留窗口 tag（CanCombo/CanBuffer）。此处会同步触发旧 WaitCombo 的
	// 关窗分支——其任务级守卫（IsSwitchingOut）会拦下对 PC 共享缓冲的 flush
	// （AbilityTask_WaitCombo::OnComboWindowChanged），不吞新角色的攻击缓冲。
	ClearCombatWindowTags();

	// 等待判定：任一配置 tag 活动（空 → 默认 Ability.Attack.Basic，资产 tag 层级匹配
	// GA_BasicAttack_01..04；tag 不能配进 CDO 构造——运行时解析，CLAUDE.md 规则 2）。
	const FZZZGameplayTags& GameplayTags = FZZZGameplayTags::Get();
	TArray<FGameplayTag> WaitTags = SwitchWaitAbilityTags;
	if (WaitTags.IsEmpty())
	{
		WaitTags.Add(GameplayTags.Ability_Attack_Basic);
	}

	bool bAnyWaitActive = false;
	for (const FGameplayTag& WaitTag : WaitTags)
	{
		if (IsAbilityActiveWithTag(WaitTag))
		{
			bAnyWaitActive = true;
			break;
		}
	}

	if (bAnyWaitActive)
	{
		// 等 GA EndAbility（取消/打断路径触发同一委托——"取消也算结束"）。
		// 5.8 委托重构：AddUObject 需 TObjectPtr，用 AddLambda（句柄由
		// UnbindSwitchOutListeners 释放，ASC 与角色同生命周期）。
		bSwitchWaitedForAbility = true;  // 攻击中切换：退场播 GA 衔接动画
		SwitchOutPhase = ESwitchOutPhase::WaitingAbilityEnd;
		WaitAbilityEndHandle = ASC->OnAbilityEnded.AddLambda(
			[this](const FAbilityEndedData&) { OnWaitAbilityEnded(); });
		// 安全阀：等待期被敌人打死（State.Dead）→ 立即结束退场，不播动画不淡出
		DeadTagHandle = ASC->RegisterGameplayTagEvent(
			GameplayTags.State_Dead, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &AZZZCharacter::OnDeadTagChanged);
		return;
	}

	TryBeginExitSequence();
}

void AZZZCharacter::TryBeginExitSequence()
{
	if (IsEliminated())
	{
		FinalizeSwitchOut();
		return;
	}

	// 打断残段（无主收刀段）前清残留窗口 tag——该段不受 GA EndAbility 兜底覆盖
	ClearCombatWindowTags();

	// 退场动画按"本次切换是否等待过攻击 GA"选择 (2026-09-01)：
	// 有攻击 GA（等其结束后退场）→ GA 衔接动画 ExitMontage；
	// 无攻击（跑步/idle 直接退场）→ RunningExitMontage。不用速度判定——技能带位移。
	UAnimMontage* ChosenExitMontage = bSwitchWaitedForAbility ? ExitMontage : RunningExitMontage;
	if (!ChosenExitMontage)
	{
		ChosenExitMontage = bSwitchWaitedForAbility ? RunningExitMontage : ExitMontage;
	}

	if (ChosenExitMontage && GetMesh() && GetMesh()->GetAnimInstance())
	{
		SwitchOutPhase = ESwitchOutPhase::ExitMontage;
		UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
		AnimInstance->Montage_Play(ChosenExitMontage, 1.0f);
		// 2026-09-01: 淡出与退场动画并行——隐藏时刻直接由 FadeDuration 控制
		//（动画开始 → FadeDuration 秒后隐藏；调 FadeDuration 即调退场节奏，
		//  与动画时长对齐则动画播完恰好隐藏）。动画结束不再介入隐藏逻辑。
		StartMaterialFade();
		return;
	}

	StartMaterialFade();
}

void AZZZCharacter::OnWaitAbilityEnded()
{
	// 重查：任一等待 tag 仍活跃则继续等（连段已被 CheckComboTransition 守卫抑制，双保险）
	const FZZZGameplayTags& GameplayTags = FZZZGameplayTags::Get();
	TArray<FGameplayTag> WaitTags = SwitchWaitAbilityTags;
	if (WaitTags.IsEmpty())
	{
		WaitTags.Add(GameplayTags.Ability_Attack_Basic);
	}
	for (const FGameplayTag& WaitTag : WaitTags)
	{
		if (IsAbilityActiveWithTag(WaitTag))
		{
			return;
		}
	}

	UnbindSwitchOutListeners();
	TryBeginExitSequence();
}

void AZZZCharacter::OnDeadTagChanged(FGameplayTag Tag, int32 NewCount)
{
	if (NewCount > 0)
	{
		// 等待期死亡：不等 GA、不播动画，直接完成退场（CancelAllAbilities 在 Finalize 内兜底）
		UnbindSwitchOutListeners();
		FinalizeSwitchOut();
	}
}

void AZZZCharacter::StartMaterialFade()
{
	if (IsEliminated())
	{
		FinalizeSwitchOut();
		return;
	}

	// 懒创建 MID 缓存（槽数变化时重建）。材质无 FadeParameterName 时
	// SetScalarParameterValue 是安全 no-op——淡出无视觉效果但仍按时序隐藏。
	if (GetMesh())
	{
		const int32 NumMaterials = GetMesh()->GetNumMaterials();
		if (FadeMaterialInstances.Num() != NumMaterials)
		{
			FadeMaterialInstances.Reset();
			for (int32 Slot = 0; Slot < NumMaterials; ++Slot)
			{
				FadeMaterialInstances.Add(GetMesh()->CreateDynamicMaterialInstance(Slot));
			}
		}
	}

	SwitchOutPhase = ESwitchOutPhase::Fading;
	FadeOpacity = 1.0f;
	GetWorldTimerManager().SetTimer(FadeTimerHandle, this, &AZZZCharacter::TickMaterialFade, 0.0167f, true);
}

void AZZZCharacter::TickMaterialFade()
{
	if (IsEliminated())
	{
		GetWorldTimerManager().ClearTimer(FadeTimerHandle);
		FinalizeSwitchOut();
		return;
	}

	FadeOpacity -= GetWorld()->GetDeltaSeconds() / FMath::Max(FadeDuration, 0.01f);
	const float ClampedOpacity = FMath::Max(0.0f, FadeOpacity);
	for (const TObjectPtr<UMaterialInstanceDynamic>& MID : FadeMaterialInstances)
	{
		if (MID)
		{
			MID->SetScalarParameterValue(FadeParameterName, ClampedOpacity);
		}
	}

	if (FadeOpacity <= 0.0f)
	{
		GetWorldTimerManager().ClearTimer(FadeTimerHandle);
		FinalizeSwitchOut();
	}
}

void AZZZCharacter::FinalizeSwitchOut()
{
	UnbindSwitchOutListeners();
	GetWorldTimerManager().ClearTimer(FadeTimerHandle);

	// 兜底：隐藏时不应有任何活动 GA（无退场动画/降级路径下非等待 GA——如闪避——可能仍在跑）
	if (ASC)
	{
		ASC->CancelAllAbilities();
	}

	// 退场无敌到此为止 — 隐藏+关碰撞后敌人 trace 打不到, 无敌不再需要。
	ClearSwitchInvulnerability();

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);

	bSwitchingOut = false;
	SwitchOutPhase = ESwitchOutPhase::Done;

	UE_LOG(LogZZZCombatRemake, Log,
		TEXT("StartSwitchOut: '%s' fully hidden — switch guard released."), *GetNameSafe(this));
	OnSwitchOutCompleted.Broadcast(this);
}

void AZZZCharacter::BeginSwitchIn()
{
	bSwitchingOut = false;
	SwitchOutPhase = ESwitchOutPhase::None;

	// 先复位透明度再显示，避免透明帧
	ResetOpacityForSwitchIn();

	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);

	// 进场窗口临时无敌 (2026-08-31): 新成员站到旧成员位置(同点)时敌人可能正在
	// 出手 — 之前 Jane 进场 3 秒被 Manny 秒杀(100 HP)。1s 后自移除(进场动画
	// play-and-forget, 窗口有界; 被玩家攻击蒙太奇覆盖则攻击本身接管)。
	ApplySwitchInvulnerability();
	GetWorldTimerManager().SetTimer(SwitchInvulnTimerHandle,
		this, &AZZZCharacter::ClearSwitchInvulnerability, 1.0f, false);

	// 进场动画 play-and-forget（被玩家攻击蒙太奇覆盖属正常）
	if (EnterMontage && GetMesh() && GetMesh()->GetAnimInstance())
	{
		GetMesh()->GetAnimInstance()->Montage_Play(EnterMontage, 1.0f);
	}
}

void AZZZCharacter::ResetOpacityForSwitchIn()
{
	// 只复位缓存 MID（淡出过才有缓存）；未淡出过则槽内材质仍是原材质，无需处理
	for (const TObjectPtr<UMaterialInstanceDynamic>& MID : FadeMaterialInstances)
	{
		if (MID)
		{
			MID->SetScalarParameterValue(FadeParameterName, 1.0f);
		}
	}
}

void AZZZCharacter::ApplySwitchInvulnerability()
{
	if (!ASC)
	{
		return;
	}

	// 已挂过(切换在途)则不动 — 退场后进场连续调用只保留一个 handle。
	if (SwitchInvulnHandle.IsValid())
	{
		return;
	}

	// Tag 应用时解析(CLAUDE.md 规则 2 — GE CDO 构造早于 tag 注册, 不能配 CDO)。
	FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(
		UZZZGameplayEffect_SwitchInvulnerable::StaticClass(), 1.0f, Ctx);
	if (Spec.IsValid())
	{
		Spec.Data->DynamicGrantedTags.AddTag(FZZZGameplayTags::Get().State_Invulnerable);
		SwitchInvulnHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}

void AZZZCharacter::ClearSwitchInvulnerability()
{
	GetWorldTimerManager().ClearTimer(SwitchInvulnTimerHandle);

	if (ASC && SwitchInvulnHandle.IsValid())
	{
		// Handle 精确移除 — 绝不 RemoveActiveEffectsWithGrantedTags: 完美闪避的
		// GE/GA 共用 State.Invulnerable, 按 tag 批量移除会误伤闪避无敌。
		ASC->RemoveActiveGameplayEffect(SwitchInvulnHandle);
		SwitchInvulnHandle = FActiveGameplayEffectHandle();
	}
}

void AZZZCharacter::ClearCombatWindowTags()
{
	if (!ASC) return;
	const FZZZGameplayTags& GameplayTags = FZZZGameplayTags::Get();
	ASC->RemoveLooseGameplayTag(GameplayTags.Effect_Ability_CanCombo);
	ASC->RemoveLooseGameplayTag(GameplayTags.Effect_Input_CanBuffer);
}

void AZZZCharacter::UnbindSwitchOutListeners()
{
	if (!ASC) return;
	if (WaitAbilityEndHandle.IsValid())
	{
		ASC->OnAbilityEnded.Remove(WaitAbilityEndHandle);
		WaitAbilityEndHandle.Reset();
	}
	if (DeadTagHandle.IsValid())
	{
		ASC->RegisterGameplayTagEvent(
			FZZZGameplayTags::Get().State_Dead, EGameplayTagEventType::NewOrRemoved)
			.Remove(DeadTagHandle);
		DeadTagHandle.Reset();
	}
}
