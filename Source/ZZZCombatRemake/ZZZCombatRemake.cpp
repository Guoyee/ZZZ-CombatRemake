// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZCombatRemake.h"
#include "Modules/ModuleManager.h"
#include "ZZZ/Tags/ZZZGameplayTags.h"

class FZZZCombatRemakeModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		// Register native GameplayTags at module load — before any asset
		// constructor (e.g. UGC_ZZZ_DamageNumber) or tag request runs.
		// Previously this lived in AZZZGameMode's constructor, which could
		// run AFTER assets were already loaded (editor startup), leaving
		// tags unresolvable and silently breaking tag-driven systems.
		FZZZGameplayTags::InitializeNativeGameplayTags();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FZZZCombatRemakeModule, ZZZCombatRemake, "ZZZCombatRemake" );

DEFINE_LOG_CATEGORY(LogZZZCombatRemake)
