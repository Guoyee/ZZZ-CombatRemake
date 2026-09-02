// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "ZZZPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class AZZZCharacter;
class UZZZDamageNumberWidget;
class UZZZDamageNumberPool;

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

	/** Pooled damage number accessor (used by the GameplayCue). */
	UZZZDamageNumberPool* GetDamageNumberPool() const { return DamageNumberPool; }

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
	/** Spawned squad instances, index-aligned with SquadClasses (hidden while inactive). */
	UPROPERTY()
	TArray<TObjectPtr<AZZZCharacter>> SquadMembers;

	/** Find a live instance of CharacterClass, or spawn one and remember it. */
	AZZZCharacter* GetOrSpawnSquadMember(UClass* CharacterClass);

	/** True if a live instance of CharacterClass currently carries State.Dead. */
	bool IsSquadClassEliminated(UClass* CharacterClass) const;

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

	/** 旧人物完全隐藏时回调——清除 bIsSwitching 守卫（AddUniqueDynamic 绑定，见 SwitchToNextCharacter）。 */
	UFUNCTION()
	void OnSwitchOutCompleted(AZZZCharacter* SwitchedOutCharacter);
};
