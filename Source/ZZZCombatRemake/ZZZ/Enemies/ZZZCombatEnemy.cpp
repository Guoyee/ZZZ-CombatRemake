// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZCombatEnemy.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "Effects/ZZZFactionGameplayEffects.h"
#include "Effects/ZZZStatusGameplayEffects.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "UI/ZZZEnemyHeadWidget.h"
#include "ZZZAttributeSet.h"
#include "ZZZCharacter.h"
#include "Tags/ZZZGameplayTags.h"
#include "ZZZCombatRemake.h"

// Test helper (PIE): `ZZZ.PauseEnemies` in the console toggles the global
// enemy-AI pause — same toggle as the number key 1 (see ZZZPlayerController).
static FAutoConsoleCommand ZZZPauseEnemiesCommand(
	TEXT("ZZZ.PauseEnemies"),
	TEXT("Toggles pause of ALL enemies' attack AI (test helper). "
		"Blocks new attacks and chasing; an in-flight attack finishes normally."),
	FConsoleCommandDelegate::CreateStatic(&AZZZCombatEnemy::ToggleGlobalPause));

bool AZZZCombatEnemy::bGlobalPauseEnabled = false;

AZZZCombatEnemy::AZZZCombatEnemy()
{
	// Keep the actor ticking: the ASC progresses Duration GEs (cooldown tags,
	// status effects) via component tick, which requires an owning tick.
	PrimaryActorTick.bCanEverTick = true;

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("ASC"));
	AttributeSet = CreateDefaultSubobject<UZZZAttributeSet>(TEXT("AttributeSet"));

	// 专属敌人胶囊通道 (2026-08-31): ZZZEnemy Profile — Block 玩家
	// (PlayerCapsule)与世界, Ignore 其他敌人与 ECC_Pawn(通道理据同
	// ZZZCharacter)。Profile 定义在 Config/DefaultEngine.ini。
	GetCapsuleComponent()->SetCollisionProfileName(TEXT("ZZZEnemy"));

	// 镜头穿透敌人 (2026-09-01): 同 ZZZCharacter — 胶囊与 mesh 对 ECC_Camera
	// 显式 Ignore, 敌人挡在镜头与玩家之间时镜头看穿, 只被世界几何遮挡。
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	// 敌人头顶状态条 (UI-Design §三, 2026-09-08): Screen 空间自动面向相机——
	// 无需任何逐帧旋转代码 (飘字 DamageWidget 同款先例)。挂根 (胶囊) 上方
	// HeadBarHeight; Widget 实例延迟到 BeginPlay 按 HeadWidgetClass 创建
	// (构造期不创建 UObject——规则 2 GE CDO 时序纪律同源)。
	HeadStatus = CreateDefaultSubobject<UWidgetComponent>(TEXT("HeadStatus"));
	HeadStatus->SetupAttachment(GetRootComponent());
	HeadStatus->SetRelativeLocation(FVector(0.0f, 0.0f, HeadBarHeight));
	HeadStatus->SetWidgetSpace(EWidgetSpace::Screen);
	// 固定像素画布 (同 DamageWidget 160x48 先例): WBP_EnemyHead 在画布内锚定布局。
	// 高度贴合内容 (名字 11pt + 条 12pt ≈ 26pt)——画布留白会让内容整体偏上
	// (内容顶对齐), 首测按观感微调 (UI-Design §八)。
	HeadStatus->SetDrawSize(FVector2D(220.0f, 40.0f));
	HeadStatus->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeadStatus->SetWindowFocusable(false);
}

UAbilitySystemComponent* AZZZCombatEnemy::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AZZZCombatEnemy::BeginPlay()
{
	Super::BeginPlay();

	// Test helper: every PIE session starts with the global enemy pause OFF.
	// The static flag outlives PIE (editor process lifetime), so a leftover
	// pause from a previous session must not leak into a fresh one.
	// (Caveat: spawning a NEW enemy mid-session also resets it — acceptable
	// for a test helper on pre-placed test enemies.)
	bGlobalPauseEnabled = false;

	if (AbilitySystemComponent)
	{
		// Single-player: Owner == Avatar == this
		AbilitySystemComponent->InitAbilityActorInfo(this, this);

		// Apply per-Blueprint stats to AttributeSet
		if (AttributeSet)
		{
			AttributeSet->SetMaxHealth(InitialMaxHealth);
			AttributeSet->SetHealth(InitialMaxHealth);
			AttributeSet->SetMaxDaze(InitialMaxDaze);
			AttributeSet->SetDaze(0.0f);
			AttributeSet->SetAttack(InitialAttack);
			AttributeSet->SetDefense(InitialDefense);
		}

		// Initial state tags: State.Alive — layer B (persistent identity),
		// granted by an Infinite GE, same pattern as the faction mark below
		// (GE-managed lifecycle instead of a manually-managed LooseTag).
		if (!AbilitySystemComponent->HasMatchingGameplayTag(FZZZGameplayTags::Get().State_Alive))
		{
			FGameplayEffectContextHandle AliveContext = AbilitySystemComponent->MakeEffectContext();
			FGameplayEffectSpecHandle AliveSpec = AbilitySystemComponent->MakeOutgoingSpec(
				UZZZGameplayEffect_Alive::StaticClass(), 1.0f, AliveContext);
			if (AliveSpec.IsValid())
			{
				AliveSpec.Data->DynamicGrantedTags.AddTag(FZZZGameplayTags::Get().State_Alive);
				AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*AliveSpec.Data.Get());
			}
		}

		// Grant the attack ability (dedupe — BeginPlay can run more than once
		// for persistent members, same pattern as AddCharacterAbilities).
		if (bEnableAttackBehavior && AttackAbilityClass
			&& !AbilitySystemComponent->FindAbilitySpecFromClass(AttackAbilityClass))
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AttackAbilityClass));
		}

		// Faction mark: State.Enemy — granted by an Infinite GE (GE-managed
		// tag lifecycle instead of a manually-managed LooseTag). Used by
		// perfect dodge / slow-motion GE targeting and switch auto-judgment.
		// Tag is added to the spec at apply time (see UZZZGameplayEffect_Faction).
		if (!AbilitySystemComponent->HasMatchingGameplayTag(FZZZGameplayTags::Get().State_Enemy))
		{
			FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
			FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(
				UZZZGameplayEffect_Faction::StaticClass(), 1.0f, Context);
			if (Spec.IsValid())
			{
				Spec.Data->DynamicGrantedTags.AddTag(FZZZGameplayTags::Get().State_Enemy);
				AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			}
		}

		// 挂点高度按 HeadBarHeight 在运行期应用 (BP 改值即刻生效, 无需重编译;
		// 构造器里那次只为编辑器预览)。
		if (HeadStatus)
		{
			HeadStatus->SetRelativeLocation(FVector(0.0f, 0.0f, HeadBarHeight));
		}

		// 敌人头顶状态条 (UI-Design §三): SetWidgetClass 在组件已 BeginPlay 后即刻
		// CreateWidget → 把本敌 ASC + 配置名交给 C++ widget (BindStatus 绑
		// Health/Daze delegate 并拉现值; 展示全在 WBP_EnemyHead)。旧 BP 未填
		// HeadWidgetClass = 组件空渲染无警告, 填了即显示。
		if (HeadWidgetClass && HeadStatus && !HeadStatus->GetWidget())
		{
			HeadStatus->SetWidgetClass(HeadWidgetClass);
			if (UZZZEnemyHeadWidget* HeadWidget =
				Cast<UZZZEnemyHeadWidget>(HeadStatus->GetWidget()))
			{
				HeadWidget->BindStatus(AbilitySystemComponent, DisplayName);
			}
		}

		// Bind health change delegate for MVP logging
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UZZZAttributeSet::GetHealthAttribute()).AddUObject(
				this, &AZZZCombatEnemy::OnHealthChanged);

		// TimeDilation bridge (Phase 3 Task 3b): GE_SlowMotion modifies the
		// TimeDilation attribute via a Duration GE — PostGameplayEffectExecute
		// does NOT fire for duration effects (design doc 10.4 / M1), so the
		// attribute → CustomTimeDilation mirror goes through the value-change
		// delegate. Both the GE apply (0.15) and its expiry (1.0) land here.
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UZZZAttributeSet::GetTimeDilationAttribute()).AddUObject(
				this, &AZZZCombatEnemy::OnTimeDilationChanged);
	}
}

void AZZZCombatEnemy::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	UE_LOG(LogZZZCombatRemake, Log,
		TEXT("[%s] Health: %.0f → %.0f"), *GetName(), Data.OldValue, Data.NewValue);

	// 头顶条整体隐藏 (UI-Design §3.2): HP==0 —— 死亡必经伤害、由本事件触发; 若未来
	// 出现"非伤害归零"用例再改绑 State.Dead tag。
	if (Data.NewValue <= 0.0f && HeadStatus)
	{
		HeadStatus->SetVisibility(false);
	}
}

void AZZZCombatEnemy::OnTimeDilationChanged(const FOnAttributeChangeData& Data)
{
	// IsNearlyEqual guard — avoid needless system updates on float jitter.
	if (FMath::IsNearlyEqual(CustomTimeDilation, Data.NewValue))
	{
		return;
	}

	UE_LOG(LogZZZCombatRemake, Log,
		TEXT("[%s] TimeDilation: %.2f → %.2f"), *GetName(), CustomTimeDilation, Data.NewValue);
	// 5.8 API fact: AActor::SetCustomTimeDilation was removed — the public
	// property is the only entry point.
	CustomTimeDilation = Data.NewValue;
}

void AZZZCombatEnemy::ToggleGlobalPause()
{
	bGlobalPauseEnabled = !bGlobalPauseEnabled;

	const FString Msg = bGlobalPauseEnabled
		? TEXT("Enemy AI PAUSED (new attacks blocked; in-flight attacks finish)")
		: TEXT("Enemy AI resumed");
	UE_LOG(LogZZZCombatRemake, Log, TEXT("[ZZZ.PauseEnemies] %s"), *Msg);

	// Screen feedback — handy mid-PIE so the state is visible without the log.
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			0, 2.0f, bGlobalPauseEnabled ? FColor::Red : FColor::Green, Msg);
	}
}

void AZZZCombatEnemy::SetPaused(bool bInPaused)
{
	bIsPaused = bInPaused;
	UE_LOG(LogZZZCombatRemake, Log, TEXT("[%s] Enemy AI %s (per-enemy)"),
		*GetName(), bIsPaused ? TEXT("PAUSED") : TEXT("resumed"));
}

void AZZZCombatEnemy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TryStartAttack(DeltaSeconds);
}

void AZZZCombatEnemy::TryStartAttack(float DeltaSeconds)
{
	// Test helper pause (number key 1 / ZZZ.PauseEnemies / SetPaused): blocks NEW
	// attacks and chasing — an in-flight attack keeps playing to the end
	// (no cancel). Wall-clock cooldown keeps ticking while paused.
	if (bGlobalPauseEnabled || bIsPaused)
	{
		return;
	}

	if (!bEnableAttackBehavior || !AbilitySystemComponent || !AttackAbilityClass)
	{
		return;
	}

	// Guards: no attacking while dead / staggered / stunned / already attacking.
	// (State.Attacking is granted by the ability's ActivationOwnedTags while a
	// swing is in flight — including its cancel path.)
	const FZZZGameplayTags& GameplayTags = FZZZGameplayTags::Get();
	FGameplayTagContainer BlockTags;
	BlockTags.AddTag(GameplayTags.State_Dead);
	BlockTags.AddTag(GameplayTags.State_Staggered);
	BlockTags.AddTag(GameplayTags.State_Stun);
	BlockTags.AddTag(GameplayTags.State_Attacking);
	if (AbilitySystemComponent->HasAnyMatchingGameplayTags(BlockTags))
	{
		FGameplayTagContainer Owned;
		AbilitySystemComponent->GetOwnedGameplayTags(Owned);
		FGameplayTagContainer Hit;
		for (const FGameplayTag& T : BlockTags)
		{
			if (Owned.HasTagExact(T))
			{
				Hit.AddTag(T);
			}
		}
		//UE_LOG(LogZZZCombatRemake, Log,
		//	TEXT("[%s] TryStartAttack blocked by tags: %s"), *GetName(), *Hit.ToStringSimple());
		return;
	}

	// Cooldown between attacks.
	const float WorldTime = GetWorld()->GetTimeSeconds();
	if (WorldTime - LastAttackTime < AttackCooldown)
	{
		return;  // cooldown log removed 2026-08-29 (spammy per-tick)
	}

	// Nearest alive, visible player within AggroRange.
	AZZZCharacter* Target = nullptr;
	float BestDistSq = AggroRange * AggroRange;
	for (TActorIterator<AZZZCharacter> It(GetWorld()); It; ++It)
	{
		AZZZCharacter* Candidate = *It;
		if (!Candidate || Candidate->IsHidden() || Candidate->IsEliminated())
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(
			GetActorLocation(), Candidate->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Target = Candidate;
		}
	}

	// In range to swing? Otherwise chase the player while inside aggro range.
	if (!Target || BestDistSq > AttackRange * AttackRange)
	{
		// UE_LOG(LogZZZCombatRemake, Log,
		// 	TEXT("[%s] TryStartAttack: %s (dist=%.0fcm, attackRange=%.0fcm)"),
		// 	*GetName(),
		// 	Target ? TEXT("chasing") : TEXT("no target"),
		// 	Target ? FMath::Sqrt(BestDistSq) : 0.0f,
		// 	AttackRange);
		if (Target)
		{
			// Chase: face the move direction (smooth yaw — the attack branch
			// snaps) and move toward the player on the XY plane.
			// (Replaced by StateTree navigation in Phase 6.)
			const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
			const FRotator FacingRot = FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f);
			SetActorRotation(FMath::RInterpTo(
				GetActorRotation(), FacingRot, DeltaSeconds, 8.0f));
			AddMovementInput(ToTarget.GetSafeNormal2D());
		}
		return;
	}

	// Face the player (yaw only — no pitching at the ground/sky).
	const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	FRotator FaceRotation = ToTarget.Rotation();
	FaceRotation.Pitch = 0.0f;
	FaceRotation.Roll = 0.0f;
	SetActorRotation(FaceRotation);

	UE_LOG(LogZZZCombatRemake, Log,
		TEXT("[%s] TryStartAttack: ATTACKING (dist=%.0fcm)"),
		*GetName(), FMath::Sqrt(BestDistSq));
	LastAttackTime = WorldTime;
	const bool bActivated =
		AbilitySystemComponent->TryActivateAbilityByClass(AttackAbilityClass);
	UE_LOG(LogZZZCombatRemake, Log,
		TEXT("[%s] TryStartAttack: TryActivateAbilityByClass -> %s"),
		*GetName(), bActivated ? TEXT("TRUE") : TEXT("FALSE"));
}

AZZZCombatEnemy* AZZZCombatEnemy::FindNearestEnemy(
	UWorld* World, const FVector& Origin, float Radius, const FGameplayTag& RequiredState)
{
	if (!World)
	{
		return nullptr;
	}

	const FZZZGameplayTags& GameplayTags = FZZZGameplayTags::Get();
	AZZZCombatEnemy* Best = nullptr;
	float BestDistSq = Radius * Radius;

	for (TActorIterator<AZZZCombatEnemy> It(World); It; ++It)
	{
		AZZZCombatEnemy* Candidate = *It;
		if (!Candidate || Candidate->IsHidden())
		{
			continue;
		}

		UAbilitySystemComponent* CandidateASC = Candidate->GetAbilitySystemComponent();
		if (!CandidateASC
			|| CandidateASC->HasMatchingGameplayTag(GameplayTags.State_Dead))
		{
			continue;  // eliminated
		}

		if (RequiredState.IsValid()
			&& !CandidateASC->HasMatchingGameplayTag(RequiredState))
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(Origin, Candidate->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Candidate;
		}
	}

	return Best;
}
