// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZCombatEnemy.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Effects/ZZZFactionGameplayEffects.h"
#include "Effects/ZZZStatusGameplayEffects.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
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
		UE_LOG(LogZZZCombatRemake, Log,
			TEXT("[%s] TryStartAttack blocked by tags: %s"), *GetName(), *Hit.ToStringSimple());
		return;
	}

	// Cooldown between attacks.
	const float WorldTime = GetWorld()->GetTimeSeconds();
	if (WorldTime - LastAttackTime < AttackCooldown)
	{
		UE_LOG(LogZZZCombatRemake, Log,
			TEXT("[%s] TryStartAttack: cooldown (%.2fs left)"), *GetName(),
			AttackCooldown - (WorldTime - LastAttackTime));
		return;
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
		UE_LOG(LogZZZCombatRemake, Log,
			TEXT("[%s] TryStartAttack: %s (dist=%.0fcm, attackRange=%.0fcm)"),
			*GetName(),
			Target ? TEXT("chasing") : TEXT("no target"),
			Target ? FMath::Sqrt(BestDistSq) : 0.0f,
			AttackRange);
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
