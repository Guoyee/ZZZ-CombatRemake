// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZGameplayTags.h"
#include "GameplayTagsManager.h"

FZZZGameplayTags FZZZGameplayTags::GameplayTagsSingleton;

void FZZZGameplayTags::InitializeNativeGameplayTags()
{
	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

	// === Input ===
	GameplayTagsSingleton.Input_Attack = Manager.AddNativeGameplayTag(
		FName("Input.Attack"), FString("Attack input"));
	GameplayTagsSingleton.Input_Switch_Next = Manager.AddNativeGameplayTag(
		FName("Input.Switch.Next"), FString("Switch to next squad member"));
	GameplayTagsSingleton.Input_Dodge = Manager.AddNativeGameplayTag(
		FName("Input.Dodge"), FString("Dodge input"));
	GameplayTagsSingleton.Input_Switch_Prev = Manager.AddNativeGameplayTag(
		FName("Input.Switch.Prev"), FString("Switch to previous squad member"));
	GameplayTagsSingleton.Input_Special = Manager.AddNativeGameplayTag(
		FName("Input.Special"), FString("Special attack input (Y key)"));

	// === Combo identity ===
	GameplayTagsSingleton.Ability_Attack_Basic_BasicAttack01 = Manager.AddNativeGameplayTag(
		FName("Ability.Attack.Basic.BasicAttack01"), FString("GA_BasicAttack_01"));
	GameplayTagsSingleton.Ability_Attack_Basic_BasicAttack02 = Manager.AddNativeGameplayTag(
		FName("Ability.Attack.Basic.BasicAttack02"), FString("GA_BasicAttack_02"));
	GameplayTagsSingleton.Ability_Attack_Basic_BasicAttack03 = Manager.AddNativeGameplayTag(
		FName("Ability.Attack.Basic.BasicAttack03"), FString("GA_BasicAttack_03"));
	GameplayTagsSingleton.Ability_Attack_Basic_BasicAttack04 = Manager.AddNativeGameplayTag(
		FName("Ability.Attack.Basic.BasicAttack04"), FString("GA_BasicAttack_04"));

	// === State ===
	GameplayTagsSingleton.State_Alive = Manager.AddNativeGameplayTag(
		FName("State.Alive"), FString("Entity is alive"));
	GameplayTagsSingleton.State_Dead = Manager.AddNativeGameplayTag(
		FName("State.Dead"), FString("Entity is dead"));
	GameplayTagsSingleton.State_Combat_Recovery = Manager.AddNativeGameplayTag(
		FName("State.Combat.Recovery"), FString("Full Recovery — move can interrupt"));
	GameplayTagsSingleton.State_Stun = Manager.AddNativeGameplayTag(
		FName("State.Stun"), FString("Entity is stunned (Daze threshold reached)"));
	GameplayTagsSingleton.State_Staggered = Manager.AddNativeGameplayTag(
		FName("State.Staggered"), FString("Entity is staggered / in hitstun"));
	GameplayTagsSingleton.State_Invulnerable = Manager.AddNativeGameplayTag(
		FName("State.Invulnerable"), FString("Invulnerable (dodge/assist window)"));
	GameplayTagsSingleton.State_SlowMotion = Manager.AddNativeGameplayTag(
		FName("State.SlowMotion"), FString("Slow motion (time fracture)"));
	GameplayTagsSingleton.State_Enemy = Manager.AddNativeGameplayTag(
		FName("State.Enemy"), FString("Enemy faction mark"));
	GameplayTagsSingleton.State_Player = Manager.AddNativeGameplayTag(
		FName("State.Player"), FString("Player faction mark"));
	GameplayTagsSingleton.State_Attacking = Manager.AddNativeGameplayTag(
		FName("State.Attacking"), FString("Enemy mid-attack (switch auto-judgment)"));
	GameplayTagsSingleton.State_PerfectDodge = Manager.AddNativeGameplayTag(
		FName("State.PerfectDodge"),
		FString("Perfect dodge flag — dodge ability lifetime; routes dodge-counter vs dash-attack"));
	GameplayTagsSingleton.State_PassThrough = Manager.AddNativeGameplayTag(
		FName("State.PassThrough"),
		FString("Pass-through window — granted by Collision Pass-Through notify; RotateToTarget stops steering"));

	// === Event ===
	GameplayTagsSingleton.Event_Combat_Hit = Manager.AddNativeGameplayTag(
		FName("Event.Combat.Hit"), FString("A hit landed on a target"));
	GameplayTagsSingleton.Event_Combat_Elimination = Manager.AddNativeGameplayTag(
		FName("Event.Combat.Elimination"), FString("Target eliminated (HP <= 0)"));
	GameplayTagsSingleton.Event_Combat_Stun = Manager.AddNativeGameplayTag(
		FName("Event.Combat.Stun"), FString("Stun threshold reached (Daze >= MaxDaze)"));
	GameplayTagsSingleton.Event_Combat_DodgePerfect = Manager.AddNativeGameplayTag(
		FName("Event.Combat.DodgePerfect"), FString("Perfect dodge triggered"));
	GameplayTagsSingleton.Event_Combat_DodgeEnd = Manager.AddNativeGameplayTag(
		FName("Event.Combat.DodgeEnd"), FString("Dodge motion section ended — ability may end, montage continues"));
	GameplayTagsSingleton.Event_Combat_DodgeSlowStart = Manager.AddNativeGameplayTag(
		FName("Event.Combat.DodgeSlowStart"),
		FString("Player slow-motion start — notify on the dodge montage's displacement tail (animator-placed)"));
	GameplayTagsSingleton.Event_Combat_ParryImpact = Manager.AddNativeGameplayTag(
		FName("Event.Combat.ParryImpact"),
		FString("Parry impact — enemy strike-frame notify broadcasts it to the player ASC; the active parry GA instance consumes it and freezes itself"));
	GameplayTagsSingleton.Event_Combat_AttackEnd = Manager.AddNativeGameplayTag(
		FName("Event.Combat.AttackEnd"), FString("Attack action section ended — ability may end, montage recovery continues"));

	// === Ability (parent tag — children registered above) ===
	GameplayTagsSingleton.Ability_Attack_Basic = Manager.AddNativeGameplayTag(
		FName("Ability.Attack.Basic"), FString("Parent tag for basic attack abilities"));
	GameplayTagsSingleton.Ability_Attack_Enemy = Manager.AddNativeGameplayTag(
		FName("Ability.Attack.Enemy"), FString("Enemy attack identity (CancelAbilities target)"));
	GameplayTagsSingleton.Ability_Defense_Dodge = Manager.AddNativeGameplayTag(
		FName("Ability.Defense.Dodge"), FString("Dodge ability"));
	GameplayTagsSingleton.Ability_Defense_Dodge_Perfect = Manager.AddNativeGameplayTag(
		FName("Ability.Defense.Dodge.Perfect"), FString("Perfect dodge"));
	GameplayTagsSingleton.Ability_Defense_Assist = Manager.AddNativeGameplayTag(
		FName("Ability.Defense.Assist"), FString("Defensive assist (parry)"));
	GameplayTagsSingleton.Ability_Switch_Quick = Manager.AddNativeGameplayTag(
		FName("Ability.Switch.Quick"), FString("Quick assist (offensive switch)"));
	GameplayTagsSingleton.Ability_Attack_Special = Manager.AddNativeGameplayTag(
		FName("Ability.Attack.Special"),
		FString("Special attack identity — spec location + self-chain guard (GA also carries Ability.Attack.Basic for cancel/wait semantics)"));
	// 追击族 (2026-09-03): parent + 段身份——镜像 Ability.Attack.Basic(.BasicAttack0X)
	// 分层; GA_DashAttack / GA_DashCounter 资产 tag 用成员 tag, 供特殊技入口规则表
	// (DashLeadContextTags) 与未来的族级取消/等待按 tag 精确匹配。
	GameplayTagsSingleton.Ability_Attack_Dash = Manager.AddNativeGameplayTag(
		FName("Ability.Attack.Dash"), FString("Dash-family parent tag (dash attack / dodge counter)"));
	GameplayTagsSingleton.Ability_Attack_Dash_Attack = Manager.AddNativeGameplayTag(
		FName("Ability.Attack.Dash.Attack"), FString("Dash attack identity (GA_DashAttack asset tag)"));
	GameplayTagsSingleton.Ability_Attack_Dash_Counter = Manager.AddNativeGameplayTag(
		FName("Ability.Attack.Dash.Counter"), FString("Dodge counter identity (GA_DashCounter asset tag)"));

	// === Effect ===
	GameplayTagsSingleton.Effect_Ability_CanCombo = Manager.AddNativeGameplayTag(
		FName("Effect.Ability.CanCombo"), FString("Combo window"));
	GameplayTagsSingleton.Effect_Input_CanBuffer = Manager.AddNativeGameplayTag(
		FName("Effect.Input.CanBuffer"), FString("Input buffer window"));
	GameplayTagsSingleton.Effect_Ability_CanDodge = Manager.AddNativeGameplayTag(
		FName("Effect.Ability.CanDodge"), FString("Perfect dodge window"));
	GameplayTagsSingleton.Effect_Ability_CanParry = Manager.AddNativeGameplayTag(
		FName("Effect.Ability.CanParry"), FString("Parry window"));
	// Effect.Ability.CanDashAttack: DEPRECATED 2026-09-03 — unified into
	// Effect.Ability.CanCombo (every attack-family window now grants CanCombo).
	// Kept registered so pre-existing assets (dodge montage AbilityWindow
	// instances, GA_DashAttack/Counter Required tags) keep loading until the
	// asset-side swap. Do NOT use in new code.
	GameplayTagsSingleton.Effect_Ability_CanDashAttack = Manager.AddNativeGameplayTag(
		FName("Effect.Ability.CanDashAttack"),
		FString("DEPRECATED 2026-09-03 — unified into Effect.Ability.CanCombo; kept registered for old assets"));
	GameplayTagsSingleton.Effect_Enemy_AttackWindow = Manager.AddNativeGameplayTag(
		FName("Effect.Enemy.AttackWindow"),
		FString("Enemy attack wind-up (yellow-flash synced) — perfect dodge + parry window"));
	GameplayTagsSingleton.Effect_Enemy_Dodged = Manager.AddNativeGameplayTag(
		FName("Effect.Enemy.Dodged"),
		FString("Attack perfectly dodged (granted by the dodge at press; consumed by the enemy-montage slow notify; attack GA EndAbility fallback)"));
	GameplayTagsSingleton.Effect_Enemy_ParryPending = Manager.AddNativeGameplayTag(
		FName("Effect.Enemy.ParryPending"),
		FString("Attack being parried (granted by the PC at the parry press; consumed by the enemy-montage parry-impact notify; attack GA EndAbility fallback)"));

	// === Camera (镜头表现 — rig 切换的 tag 驱动, 2026-09-11) ===
	GameplayTagsSingleton.Camera_Closeup_Parry = Manager.AddNativeGameplayTag(
		FName("Camera.Closeup.Parry"),
		FString("Parry closeup camera (put on the parry GA's ActivationOwnedTags; UZZZTagCameraDirector maps it to a rig)"));

	// === SetByCaller Data ===
	GameplayTagsSingleton.Data_Damage = Manager.AddNativeGameplayTag(
		FName("Data.Damage"), FString("SetByCaller: base damage before defense scaling"));
	GameplayTagsSingleton.Data_Daze = Manager.AddNativeGameplayTag(
		FName("Data.Daze"), FString("SetByCaller: absolute daze buildup amount"));
	GameplayTagsSingleton.Data_Energy = Manager.AddNativeGameplayTag(
		FName("Data.Energy"), FString("SetByCaller: energy delta (+gain / -cost)"));
	// NOTE: Data.Energy ALSO lives in DefaultGameplayTags.ini — the energy GE
	// CDO (UZZZGameplayEffect_EnergyDelta) parses it in its ctor, before native
	// tags register. Without the ini entry the modifier silently evaluates to 0.

	// === GameplayCue ===
	// Declared in DefaultGameplayTags.ini (NOT AddNativeGameplayTag) so the tag
	// exists before any class CDO construction — the cue class reads it in its
	// ctor, which can run during module static init (before StartupModule).
	// Fall back to AddNativeGameplayTag if the ini entry is missing.
	{
		const FGameplayTag IniCueTag =
			FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.DamageNumber"), false);
		GameplayTagsSingleton.GameplayCue_ZZZ_DamageNumber = IniCueTag.IsValid()
			? IniCueTag
			: Manager.AddNativeGameplayTag(
				FName("GameplayCue.ZZZ.DamageNumber"), FString("Damage number popup"));
	}
	{
		const FGameplayTag IniCueTag =
			FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.CameraShake"), false);
		GameplayTagsSingleton.GameplayCue_ZZZ_CameraShake = IniCueTag.IsValid()
			? IniCueTag
			: Manager.AddNativeGameplayTag(
				FName("GameplayCue.ZZZ.CameraShake"), FString("Camera shake"));
	}
	{
		const FGameplayTag IniCueTag =
			FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.CameraShake.Low"), false);
		GameplayTagsSingleton.GameplayCue_ZZZ_CameraShakeLow = IniCueTag.IsValid()
			? IniCueTag
			: Manager.AddNativeGameplayTag(
				FName("GameplayCue.ZZZ.CameraShake.Low"), FString("Camera shake — low tier"));
	}
	{
		const FGameplayTag IniCueTag =
			FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.CameraShake.Mid"), false);
		GameplayTagsSingleton.GameplayCue_ZZZ_CameraShakeMid = IniCueTag.IsValid()
			? IniCueTag
			: Manager.AddNativeGameplayTag(
				FName("GameplayCue.ZZZ.CameraShake.Mid"), FString("Camera shake — mid tier (default)"));
	}
	{
		const FGameplayTag IniCueTag =
			FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.CameraShake.High"), false);
		GameplayTagsSingleton.GameplayCue_ZZZ_CameraShakeHigh = IniCueTag.IsValid()
			? IniCueTag
			: Manager.AddNativeGameplayTag(
				FName("GameplayCue.ZZZ.CameraShake.High"), FString("Camera shake — high tier"));
	}
	{
		const FGameplayTag IniCueTag =
			FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.EnemyAttackWarning"), false);
		GameplayTagsSingleton.GameplayCue_ZZZ_EnemyAttackWarning = IniCueTag.IsValid()
			? IniCueTag
			: Manager.AddNativeGameplayTag(
				FName("GameplayCue.ZZZ.EnemyAttackWarning"), FString("Enemy attack wind-up warning flash"));
	}
	{
		const FGameplayTag IniCueTag =
			FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.ParryImpact"), false);
		GameplayTagsSingleton.GameplayCue_ZZZ_ParryImpact = IniCueTag.IsValid()
			? IniCueTag
			: Manager.AddNativeGameplayTag(
				FName("GameplayCue.ZZZ.ParryImpact"), FString("Parry impact flash at the enemy strike frame"));
	}
	{
		const FGameplayTag IniCueTag =
			FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.Sound.BasicAttack01.Swing1"), false);
		GameplayTagsSingleton.GameplayCue_ZZZ_Sound_BasicAttack01_Swing1 = IniCueTag.IsValid()
			? IniCueTag
			: Manager.AddNativeGameplayTag(
				FName("GameplayCue.ZZZ.Sound.BasicAttack01.Swing1"), FString("BA01 first swing whoosh (one-shot)"));
	}
	{
		const FGameplayTag IniCueTag =
			FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.Sound.BasicAttack01.Swing2"), false);
		GameplayTagsSingleton.GameplayCue_ZZZ_Sound_BasicAttack01_Swing2 = IniCueTag.IsValid()
			? IniCueTag
			: Manager.AddNativeGameplayTag(
				FName("GameplayCue.ZZZ.Sound.BasicAttack01.Swing2"), FString("BA01 second swing whoosh (one-shot)"));
	}
	{
		const FGameplayTag IniCueTag =
			FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.Sound.BasicAttack02.Boom"), false);
		GameplayTagsSingleton.GameplayCue_ZZZ_Sound_BasicAttack02_Boom = IniCueTag.IsValid()
			? IniCueTag
			: Manager.AddNativeGameplayTag(
				FName("GameplayCue.ZZZ.Sound.BasicAttack02.Boom"), FString("BA02 heavy hit boom (one-shot)"));
	}
	{
		const FGameplayTag IniCueTag =
			FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.Sound.BasicAttack03.Swing1"), false);
		GameplayTagsSingleton.GameplayCue_ZZZ_Sound_BasicAttack03_Swing1 = IniCueTag.IsValid()
			? IniCueTag
			: Manager.AddNativeGameplayTag(
				FName("GameplayCue.ZZZ.Sound.BasicAttack03.Swing1"), FString("BA03 first swing whoosh (one-shot)"));
	}
	{
		const FGameplayTag IniCueTag =
			FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.Sound.BasicAttack03.Swing2"), false);
		GameplayTagsSingleton.GameplayCue_ZZZ_Sound_BasicAttack03_Swing2 = IniCueTag.IsValid()
			? IniCueTag
			: Manager.AddNativeGameplayTag(
				FName("GameplayCue.ZZZ.Sound.BasicAttack03.Swing2"), FString("BA03 second swing whoosh (one-shot)"));
	}
	{
		const FGameplayTag IniCueTag =
			FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.Sound.BasicAttack03.Swing3"), false);
		GameplayTagsSingleton.GameplayCue_ZZZ_Sound_BasicAttack03_Swing3 = IniCueTag.IsValid()
			? IniCueTag
			: Manager.AddNativeGameplayTag(
				FName("GameplayCue.ZZZ.Sound.BasicAttack03.Swing3"), FString("BA03 third swing whoosh (one-shot)"));
	}
	{
		const FGameplayTag IniCueTag =
			FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.Sound.BasicAttack04.Swing1"), false);
		GameplayTagsSingleton.GameplayCue_ZZZ_Sound_BasicAttack04_Swing1 = IniCueTag.IsValid()
			? IniCueTag
			: Manager.AddNativeGameplayTag(
				FName("GameplayCue.ZZZ.Sound.BasicAttack04.Swing1"), FString("BA04 first swing whoosh (one-shot)"));
	}
	{
		const FGameplayTag IniCueTag =
			FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.Sound.BasicAttack04.Swing2"), false);
		GameplayTagsSingleton.GameplayCue_ZZZ_Sound_BasicAttack04_Swing2 = IniCueTag.IsValid()
			? IniCueTag
			: Manager.AddNativeGameplayTag(
				FName("GameplayCue.ZZZ.Sound.BasicAttack04.Swing2"), FString("BA04 second swing whoosh (one-shot)"));
	}
	{
		const FGameplayTag IniCueTag =
			FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.Sound.BasicAttack04.Swing3"), false);
		GameplayTagsSingleton.GameplayCue_ZZZ_Sound_BasicAttack04_Swing3 = IniCueTag.IsValid()
			? IniCueTag
			: Manager.AddNativeGameplayTag(
				FName("GameplayCue.ZZZ.Sound.BasicAttack04.Swing3"), FString("BA04 third swing whoosh (one-shot)"));
	}
	{
		const FGameplayTag IniCueTag =
			FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.Sound.Dash"), false);
		GameplayTagsSingleton.GameplayCue_ZZZ_Sound_Dash = IniCueTag.IsValid()
			? IniCueTag
			: Manager.AddNativeGameplayTag(
				FName("GameplayCue.ZZZ.Sound.Dash"), FString("Dodge displacement whoosh (one-shot)"));
	}
	{
		const FGameplayTag IniCueTag =
			FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.Sound.DashAttack"), false);
		GameplayTagsSingleton.GameplayCue_ZZZ_Sound_DashAttack = IniCueTag.IsValid()
			? IniCueTag
			: Manager.AddNativeGameplayTag(
				FName("GameplayCue.ZZZ.Sound.DashAttack"), FString("Dash attack swing whoosh (one-shot)"));
	}
	{
		const FGameplayTag IniCueTag =
			FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.Sound.DashCounter.01"), false);
		GameplayTagsSingleton.GameplayCue_ZZZ_Sound_DashCounter_01 = IniCueTag.IsValid()
			? IniCueTag
			: Manager.AddNativeGameplayTag(
				FName("GameplayCue.ZZZ.Sound.DashCounter.01"), FString("Dodge counter sound 01 (one-shot)"));
	}
	{
		const FGameplayTag IniCueTag =
			FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.Sound.DashCounter.02"), false);
		GameplayTagsSingleton.GameplayCue_ZZZ_Sound_DashCounter_02 = IniCueTag.IsValid()
			? IniCueTag
			: Manager.AddNativeGameplayTag(
				FName("GameplayCue.ZZZ.Sound.DashCounter.02"), FString("Dodge counter sound 02 (one-shot)"));
	}
	{
		const FGameplayTag IniCueTag =
			FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.Sound.DashCounter.03"), false);
		GameplayTagsSingleton.GameplayCue_ZZZ_Sound_DashCounter_03 = IniCueTag.IsValid()
			? IniCueTag
			: Manager.AddNativeGameplayTag(
				FName("GameplayCue.ZZZ.Sound.DashCounter.03"), FString("Dodge counter sound 03 (one-shot)"));
	}
	{
		const FGameplayTag IniCueTag =
			FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.Sound.DashCounter.04"), false);
		GameplayTagsSingleton.GameplayCue_ZZZ_Sound_DashCounter_04 = IniCueTag.IsValid()
			? IniCueTag
			: Manager.AddNativeGameplayTag(
				FName("GameplayCue.ZZZ.Sound.DashCounter.04"), FString("Dodge counter sound 04 (one-shot)"));
	}
	{
		const FGameplayTag IniCueTag =
			FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.Sound.Jane.Swing1"), false);
		GameplayTagsSingleton.GameplayCue_ZZZ_Sound_Jane_Swing1 = IniCueTag.IsValid()
			? IniCueTag
			: Manager.AddNativeGameplayTag(
				FName("GameplayCue.ZZZ.Sound.Jane.Swing1"), FString("Jane swing sound 1 (reusable asset)"));
	}
	{
		const FGameplayTag IniCueTag =
			FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.Sound.Jane.Swing2"), false);
		GameplayTagsSingleton.GameplayCue_ZZZ_Sound_Jane_Swing2 = IniCueTag.IsValid()
			? IniCueTag
			: Manager.AddNativeGameplayTag(
				FName("GameplayCue.ZZZ.Sound.Jane.Swing2"), FString("Jane swing sound 2 (reusable asset)"));
	}
	{
		const FGameplayTag IniCueTag =
			FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.Sound.Jane.Swing3"), false);
		GameplayTagsSingleton.GameplayCue_ZZZ_Sound_Jane_Swing3 = IniCueTag.IsValid()
			? IniCueTag
			: Manager.AddNativeGameplayTag(
				FName("GameplayCue.ZZZ.Sound.Jane.Swing3"), FString("Jane swing sound 3 (reusable asset)"));
	}
	{
		const FGameplayTag IniCueTag =
			FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.Sound.Jane.Swing4"), false);
		GameplayTagsSingleton.GameplayCue_ZZZ_Sound_Jane_Swing4 = IniCueTag.IsValid()
			? IniCueTag
			: Manager.AddNativeGameplayTag(
				FName("GameplayCue.ZZZ.Sound.Jane.Swing4"), FString("Jane swing sound 4 (reusable asset)"));
	}
	{
		const FGameplayTag IniCueTag =
			FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.Sound.Jane.Swing5"), false);
		GameplayTagsSingleton.GameplayCue_ZZZ_Sound_Jane_Swing5 = IniCueTag.IsValid()
			? IniCueTag
			: Manager.AddNativeGameplayTag(
				FName("GameplayCue.ZZZ.Sound.Jane.Swing5"), FString("Jane swing sound 5 (reusable asset)"));
	}
	{
		const FGameplayTag IniCueTag =
			FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.Sound.Jane.BasicAttack04.1"), false);
		GameplayTagsSingleton.GameplayCue_ZZZ_Sound_Jane_BasicAttack04_1 = IniCueTag.IsValid()
			? IniCueTag
			: Manager.AddNativeGameplayTag(
				FName("GameplayCue.ZZZ.Sound.Jane.BasicAttack04.1"), FString("Jane special basic attack sound 1/2 (one-shot)"));
	}
	{
		const FGameplayTag IniCueTag =
			FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.Sound.Jane.BasicAttack04.2"), false);
		GameplayTagsSingleton.GameplayCue_ZZZ_Sound_Jane_BasicAttack04_2 = IniCueTag.IsValid()
			? IniCueTag
			: Manager.AddNativeGameplayTag(
				FName("GameplayCue.ZZZ.Sound.Jane.BasicAttack04.2"), FString("Jane special basic attack sound 2/2 (one-shot)"));
	}
	{
		const FGameplayTag IniCueTag =
			FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.Sound.Hit.1"), false);
		GameplayTagsSingleton.GameplayCue_ZZZ_Sound_Hit_1 = IniCueTag.IsValid()
			? IniCueTag
			: Manager.AddNativeGameplayTag(
				FName("GameplayCue.ZZZ.Sound.Hit.1"), FString("Hit confirmation sound 1 on target (global, one-shot)"));
	}
	{
		const FGameplayTag IniCueTag =
			FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.Sound.Hit.2"), false);
		GameplayTagsSingleton.GameplayCue_ZZZ_Sound_Hit_2 = IniCueTag.IsValid()
			? IniCueTag
			: Manager.AddNativeGameplayTag(
				FName("GameplayCue.ZZZ.Sound.Hit.2"), FString("Hit confirmation sound 2 on target (global, one-shot)"));
	}
	{
		const FGameplayTag IniCueTag =
			FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.Sound.Hit.3"), false);
		GameplayTagsSingleton.GameplayCue_ZZZ_Sound_Hit_3 = IniCueTag.IsValid()
			? IniCueTag
			: Manager.AddNativeGameplayTag(
				FName("GameplayCue.ZZZ.Sound.Hit.3"), FString("Hit confirmation sound 3 on target (global, one-shot)"));
	}
	{
		const FGameplayTag IniCueTag =
			FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.Sound.Hit.4"), false);
		GameplayTagsSingleton.GameplayCue_ZZZ_Sound_Hit_4 = IniCueTag.IsValid()
			? IniCueTag
			: Manager.AddNativeGameplayTag(
				FName("GameplayCue.ZZZ.Sound.Hit.4"), FString("Hit confirmation sound 4 on target (global, one-shot)"));
	}
	{
		const FGameplayTag IniCueTag =
			FGameplayTag::RequestGameplayTag(FName("GameplayCue.ZZZ.Sound.Hit.5"), false);
		GameplayTagsSingleton.GameplayCue_ZZZ_Sound_Hit_5 = IniCueTag.IsValid()
			? IniCueTag
			: Manager.AddNativeGameplayTag(
				FName("GameplayCue.ZZZ.Sound.Hit.5"), FString("Hit confirmation sound 5 on target (global, one-shot)"));
	}
}
