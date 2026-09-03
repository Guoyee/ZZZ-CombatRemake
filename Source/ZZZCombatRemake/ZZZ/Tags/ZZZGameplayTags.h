// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/**
 * ZZZ native GameplayTags — singleton.
 *
 * Pattern: add a member field below + one line in InitializeNativeGameplayTags().
 * No macros, no .h/.cpp sync required.
 *
 * Usage:
 *   FZZZGameplayTags::Get().Input_Attack
 *   FZZZGameplayTags::Get().Effect_Ability_CanCombo
 */
struct FZZZGameplayTags
{
public:
	static const FZZZGameplayTags& Get() { return GameplayTagsSingleton; }
	static void InitializeNativeGameplayTags();

	// === Input ===
	FGameplayTag Input_Attack;
	FGameplayTag Input_Switch_Next;  // "Input.Switch.Next"
	FGameplayTag Input_Dodge;        // "Input.Dodge"
	FGameplayTag Input_Switch_Prev;  // "Input.Switch.Prev"
	FGameplayTag Input_Special;      // "Input.Special" (特殊技 Y 键)

	// === Ability ===
	FGameplayTag Ability_Attack_Basic;
	FGameplayTag Ability_Attack_Enemy;             // "Ability.Attack.Enemy" (敌人攻击身份，供 CancelAbilities 定位)
	FGameplayTag Ability_Attack_Special;           // "Ability.Attack.Special" (特殊技身份 — 定位 spec + 自链防护)
	FGameplayTag Ability_Attack_Dash;              // "Ability.Attack.Dash" (追击族父 tag——冲刺攻击/闪避反击, 2026-09-03)
	FGameplayTag Ability_Attack_Dash_Attack;       // "Ability.Attack.Dash.Attack" (冲刺攻击段身份 — GA_DashAttack 资产 tag)
	FGameplayTag Ability_Attack_Dash_Counter;      // "Ability.Attack.Dash.Counter" (闪避反击段身份 — GA_DashCounter 资产 tag)
	FGameplayTag Ability_Defense_Dodge;            // "Ability.Defense.Dodge"
	FGameplayTag Ability_Defense_Dodge_Perfect;    // "Ability.Defense.Dodge.Perfect"
	FGameplayTag Ability_Defense_Assist;           // "Ability.Defense.Assist" (弹刀)
	FGameplayTag Ability_Switch_Quick;             // "Ability.Switch.Quick" (突击支援)

	// === Combo identity (hierarchical children of Ability.Attack.Basic) ===
	FGameplayTag Ability_Attack_Basic_BasicAttack01;
	FGameplayTag Ability_Attack_Basic_BasicAttack02;
	FGameplayTag Ability_Attack_Basic_BasicAttack03;
	FGameplayTag Ability_Attack_Basic_BasicAttack04;

	// === State ===
	FGameplayTag State_Alive;            // "State.Alive"
	FGameplayTag State_Dead;             // "State.Dead"
	FGameplayTag State_Combat_Recovery;  // "State.Combat.Recovery"
	FGameplayTag State_Stun;             // "State.Stun"
	FGameplayTag State_Staggered;        // "State.Staggered"
	FGameplayTag State_Invulnerable;     // "State.Invulnerable" (闪避/弹刀窗口无敌)
	FGameplayTag State_SlowMotion;       // "State.SlowMotion" (时空断裂)
	FGameplayTag State_Enemy;            // "State.Enemy" (敌人阵营标记)
	FGameplayTag State_Player;           // "State.Player" (玩家阵营标记)
	FGameplayTag State_Attacking;        // "State.Attacking" (敌人攻击中 — 切换自动判定)
	FGameplayTag State_PerfectDodge;     // "State.PerfectDodge" (完美闪避标志 — 闪避能力生命周期，闪避反击路由)
	FGameplayTag State_PassThrough;      // "State.PassThrough" (穿透窗口 — notify 配对授予，RotateToTarget 停止索敌转向)

	// === Event ===
	FGameplayTag Event_Combat_Hit;         // "Event.Combat.Hit"
	FGameplayTag Event_Combat_Elimination; // "Event.Combat.Elimination"
	FGameplayTag Event_Combat_Stun;        // "Event.Combat.Stun"
	FGameplayTag Event_Combat_DodgePerfect; // "Event.Combat.DodgePerfect"
	FGameplayTag Event_Combat_DodgeEnd;     // "Event.Combat.DodgeEnd" (前段结束，Notify 广播)
	FGameplayTag Event_Combat_DodgeSlowStart; // "Event.Combat.DodgeSlowStart" (玩家慢放开始——位移末段 Notify 广播，动画师摆放)
	FGameplayTag Event_Combat_AttackEnd;    // "Event.Combat.AttackEnd" (攻击主体结束，Notify 广播；区别于 State.Combat.Recovery)

	// === Effect ===
	FGameplayTag Effect_Ability_CanCombo;     // "Effect.Ability.CanCombo" (通用"可输入下一动作"窗口——普攻连段/闪避位移追击/冲刺攻击尾/特殊技入口共用, 2026-09-03 统一)
	FGameplayTag Effect_Input_CanBuffer;      // "Effect.Input.CanBuffer"
	FGameplayTag Effect_Ability_CanDodge;     // "Effect.Ability.CanDodge" (完美闪避窗口——已废弃)
	FGameplayTag Effect_Ability_CanParry;     // "Effect.Ability.CanParry" (弹刀窗口——已废弃)
	FGameplayTag Effect_Ability_CanDashAttack; // "Effect.Ability.CanDashAttack" (已废弃 2026-09-03——统一并入 Effect.Ability.CanCombo；保留注册防旧资产报错，勿用)
	FGameplayTag Effect_Enemy_AttackWindow;   // "Effect.Enemy.AttackWindow" (敌人攻击前摇黄闪期——极限闪避 + 弹刀共用窗口)

	// === SetByCaller Data ===
	FGameplayTag Data_Damage;          // "Data.Damage"
	FGameplayTag Data_Daze;            // "Data.Daze"
	FGameplayTag Data_Energy;          // "Data.Energy" (能量增量 SetByCaller —— 需 ini 预注册，能量 GE CDO 构造期解析)

	// === GameplayCue ===
	FGameplayTag GameplayCue_ZZZ_DamageNumber;       // "GameplayCue.ZZZ.DamageNumber"
	FGameplayTag GameplayCue_ZZZ_CameraShake;        // "GameplayCue.ZZZ.CameraShake" (原生基类默认 tag——BP 子类各自覆盖为 Low/Mid/High)
	FGameplayTag GameplayCue_ZZZ_CameraShakeLow;     // "GameplayCue.ZZZ.CameraShake.Low" (轻震)
	FGameplayTag GameplayCue_ZZZ_CameraShakeMid;     // "GameplayCue.ZZZ.CameraShake.Mid" (默认档)
	FGameplayTag GameplayCue_ZZZ_CameraShakeHigh;    // "GameplayCue.ZZZ.CameraShake.High" (重震)
	FGameplayTag GameplayCue_ZZZ_EnemyAttackWarning; // "GameplayCue.ZZZ.EnemyAttackWarning" (敌人攻击前摇黄闪)
	// === 音效（一次性挥击/重击，蒙太奇帧触发 — 每个音频文件一个 tag） ===
	FGameplayTag GameplayCue_ZZZ_Sound_BasicAttack01_Swing1;  // "GameplayCue.ZZZ.Sound.BasicAttack01.Swing1"
	FGameplayTag GameplayCue_ZZZ_Sound_BasicAttack01_Swing2;  // "GameplayCue.ZZZ.Sound.BasicAttack01.Swing2"
	FGameplayTag GameplayCue_ZZZ_Sound_BasicAttack02_Boom;    // "GameplayCue.ZZZ.Sound.BasicAttack02.Boom"
	FGameplayTag GameplayCue_ZZZ_Sound_BasicAttack03_Swing1;  // "GameplayCue.ZZZ.Sound.BasicAttack03.Swing1"
	FGameplayTag GameplayCue_ZZZ_Sound_BasicAttack03_Swing2;  // "GameplayCue.ZZZ.Sound.BasicAttack03.Swing2"
	FGameplayTag GameplayCue_ZZZ_Sound_BasicAttack03_Swing3;  // "GameplayCue.ZZZ.Sound.BasicAttack03.Swing3"
	FGameplayTag GameplayCue_ZZZ_Sound_BasicAttack04_Swing1;  // "GameplayCue.ZZZ.Sound.BasicAttack04.Swing1"
	FGameplayTag GameplayCue_ZZZ_Sound_BasicAttack04_Swing2;  // "GameplayCue.ZZZ.Sound.BasicAttack04.Swing2"
	FGameplayTag GameplayCue_ZZZ_Sound_BasicAttack04_Swing3;  // "GameplayCue.ZZZ.Sound.BasicAttack04.Swing3"
	FGameplayTag GameplayCue_ZZZ_Sound_Dash;                  // "GameplayCue.ZZZ.Sound.Dash" (闪避位移风声)
	FGameplayTag GameplayCue_ZZZ_Sound_DashAttack;            // "GameplayCue.ZZZ.Sound.DashAttack" (冲刺攻击挥击音)
	FGameplayTag GameplayCue_ZZZ_Sound_DashCounter_01;        // "GameplayCue.ZZZ.Sound.DashCounter.01"
	FGameplayTag GameplayCue_ZZZ_Sound_DashCounter_02;        // "GameplayCue.ZZZ.Sound.DashCounter.02"
	FGameplayTag GameplayCue_ZZZ_Sound_DashCounter_03;        // "GameplayCue.ZZZ.Sound.DashCounter.03"
	FGameplayTag GameplayCue_ZZZ_Sound_DashCounter_04;        // "GameplayCue.ZZZ.Sound.DashCounter.04"
	// === Jane（素材库式挥动音，跨技能复用） ===
	FGameplayTag GameplayCue_ZZZ_Sound_Jane_Swing1;           // "GameplayCue.ZZZ.Sound.Jane.Swing1"
	FGameplayTag GameplayCue_ZZZ_Sound_Jane_Swing2;           // "GameplayCue.ZZZ.Sound.Jane.Swing2"
	FGameplayTag GameplayCue_ZZZ_Sound_Jane_Swing3;           // "GameplayCue.ZZZ.Sound.Jane.Swing3"
	FGameplayTag GameplayCue_ZZZ_Sound_Jane_Swing4;           // "GameplayCue.ZZZ.Sound.Jane.Swing4"
	FGameplayTag GameplayCue_ZZZ_Sound_Jane_Swing5;           // "GameplayCue.ZZZ.Sound.Jane.Swing5"
	FGameplayTag GameplayCue_ZZZ_Sound_Jane_BasicAttack04_1;  // "GameplayCue.ZZZ.Sound.Jane.BasicAttack04.1" (特殊普攻 1/2)
	FGameplayTag GameplayCue_ZZZ_Sound_Jane_BasicAttack04_2;  // "GameplayCue.ZZZ.Sound.Jane.BasicAttack04.2" (特殊普攻 2/2)
	FGameplayTag GameplayCue_ZZZ_Sound_Hit_1;                // "GameplayCue.ZZZ.Sound.Hit.1" (命中音 1 — 全局通用)
	FGameplayTag GameplayCue_ZZZ_Sound_Hit_2;                // "GameplayCue.ZZZ.Sound.Hit.2" (命中音 2)
	FGameplayTag GameplayCue_ZZZ_Sound_Hit_3;                // "GameplayCue.ZZZ.Sound.Hit.3" (命中音 3)
	FGameplayTag GameplayCue_ZZZ_Sound_Hit_4;                // "GameplayCue.ZZZ.Sound.Hit.4" (命中音 4)
	FGameplayTag GameplayCue_ZZZ_Sound_Hit_5;                // "GameplayCue.ZZZ.Sound.Hit.5" (命中音 5)

private:
	static FZZZGameplayTags GameplayTagsSingleton;

	FZZZGameplayTags() = default;
	FZZZGameplayTags(const FZZZGameplayTags&) = delete;
	FZZZGameplayTags& operator=(const FZZZGameplayTags&) = delete;
};
