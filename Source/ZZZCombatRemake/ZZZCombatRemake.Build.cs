// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ZZZCombatRemake : ModuleRules
{
	public ZZZCombatRemake(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"GameplayCameras",
			"MotionWarping"  // 招架/突击 root motion 扭曲 (2026-09-06)
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Niagara",  // GC_ZZZ_EnemyAttackWarning (UNiagaraFunctionLibrary)
		});

		PublicIncludePaths.AddRange(new string[] {
			"ZZZCombatRemake",
			"ZZZCombatRemake/ZZZ",
			"ZZZCombatRemake/ZZZ/Abilities",
			"ZZZCombatRemake/ZZZ/Animation",
			"ZZZCombatRemake/ZZZ/Attributes",
			"ZZZCombatRemake/ZZZ/Enemies",
			"ZZZCombatRemake/ZZZ/Game",
			"ZZZCombatRemake/ZZZ/Input",
			"ZZZCombatRemake/ZZZ/Player",
			"ZZZCombatRemake/ZZZ/Tags",
			"ZZZCombatRemake/ZZZ/Tasks",
			"ZZZCombatRemake/ZZZ/Cues",
			"ZZZCombatRemake/ZZZ/UI",
			"ZZZCombatRemake/Variant_Platforming",
			"ZZZCombatRemake/Variant_Platforming/Animation",
			"ZZZCombatRemake/Variant_Combat",
			"ZZZCombatRemake/Variant_Combat/AI",
			"ZZZCombatRemake/Variant_Combat/Animation",
			"ZZZCombatRemake/Variant_Combat/Gameplay",
			"ZZZCombatRemake/Variant_Combat/Interfaces",
			"ZZZCombatRemake/Variant_Combat/UI",
			"ZZZCombatRemake/Variant_SideScrolling",
			"ZZZCombatRemake/Variant_SideScrolling/AI",
			"ZZZCombatRemake/Variant_SideScrolling/Gameplay",
			"ZZZCombatRemake/Variant_SideScrolling/Interfaces",
			"ZZZCombatRemake/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
