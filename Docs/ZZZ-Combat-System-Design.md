# 绝区零核心战斗逻辑复刻 — 架构设计文档

> **项目**: ZZZCombatRemake (UE 5.8) · 单机 · DX12/SM6
> **目标**: 在现有 ThirdPerson 基础上复刻《绝区零》核心战斗逻辑
> **状态**: Phase 1/1.5/2 完成，Phase 3 进行中
> **旧版备份**: `Docs/archive/ZZZ-Combat-System-Design.md.orig`（勿读，仅查证历史）

## 一、目标系统

| 系统 | 说明 | 状态 |
|---|---|---|
| 基础攻击连段 | 3-5 段普攻，预输入缓冲 | ✅ Phase 2（4 段） |
| 特殊攻击 EX | 普通/强化特殊技（Y；能量 ≥ 消耗触发强化版）+ 能量系统 | ✅ 2026-09-03（C++ ✅；资产待做） |
| 闪避 / 完美闪避 | 极限时机触发时空断裂 | 🔄 Phase 3 |
| 弹刀 / 突击支援 | 防御/进攻型角色切换 | 🔄 Phase 3 |
| 终结技 | 消耗全队共享 Decibel | ⬜ Phase 5 |
| 连携技 Chain Attack | 打空 Daze 后多人连携 | ⬜ Phase 5 |
| 元素 / 异常 | Fire/Ice/Electric/Physical/Ether + 积蓄爆发 | ⬜ Phase 4 |
| Daze 失衡 | 攻击积累架势伤害，满后失衡 | ✅ 数值（连携 ⬜ Phase 5） |
| 三人编队切换 | 持久化成员 + 异步退场状态机（等 GA 结束→退场动画→材质淡出→隐藏）| 🔄 普通切换已实现；弹刀/突击自动判定 ⬜ |
| 敌人 AI | 以骸行为 | ⬜ Phase 6（最小攻击已实现） |

## 二、核心架构决策

1. **GAS 是架构核心**：Ability/GE/Tag/Attribute 替代模板的"手动 Montage + 接口回调"。
2. **增量替换**：新建 `ZZZ/` 模块（Source + Content），`Variant_*` 模板保留作参考，不删除。
3. **Pawn-ASC（2026-08-01）**：玩家与敌人统一——每个 Pawn 自带 ASC + AttributeSet，`BeginPlay` 时 `InitAbilityActorInfo(this, this)`，与 Possess 解耦；隐藏编队成员继续 Tick（冷却/后台 DoT，`PrimaryActorTick.bCanEverTick=true` 必须）。`AZZZPlayerState` 仅作团队共享数据宿主（Phase 5 Decibel）。理由：独立血条/资源/冷却天然隔离，单机无复制需求；全队共享资源另放 GameState/Director。
4. **模块**：当前单模块 `Source/ZZZCombatRemake/`；未来按需拆 `ZZZCombatCore`（GAS 核心），依赖方向 Core 不依赖 AI/UI。
5. **角色差异化**：一个 C++ 基类 + 每角色 BP 子类 + 每角色自己的 GA BP 资产（`DefaultAbilities`）；输入配置全局共享。不建 per-character C++ 类，除非角色有独特机制。
6. **Lyra 风格输入**：`UZZZInputConfig` DataAsset 把 InputAction → GameplayTag；统一绑定 → `Input_AbilityInputTagPressed` → ASC `HandleGameplayEvent` + 能力激活。
7. **伤害统一 ExecCalc**：一次计算 HP 伤害 + Daze 积蓄 + Anomaly 积蓄。
8. **视觉反馈走 GameplayCue**：禁止在 `PostGameplayEffectExecute` 中 SpawnEmitter。

## 三、类层级

### 3.1 Character

```
ACharacter
└── AZZZCombatRemakeCharacter（最底层抽象基类）
    ├── AZZZCharacter          # 玩家：Pawn-ASC、输入→Tag 路由、Move 打断
    │   └── BP 子类（BP_Okuma…）：网格/动画/DefaultAbilities/输入配置
    └── AZZZCombatEnemy        # 敌人：Pawn-ASC、Tick 驱动攻击、受击硬直
        └── BP 子类（BP_EnemyTest_Manny…）
```

`AZZZCharacter` 职责：移动/相机、ASC 初始化（含阵营 GE `State.Player`）、`DefaultAbilities` 授予（按 class 去重）、输入绑定、**切换进出场状态机**（`BeginSwitchIn` / `StartSwitchOut`，见 §4.9.1）。切换由 PC 编排（`SwitchToNextCharacter`）：新人物立即进场（不等旧人物），旧人物异步退场（等攻击 GA EndAbility → 退场动画 → 材质淡出 → 隐藏），全程不 Cancel 旧能力（脱手技语义）。

### 3.2 Ability

```
UZZZGameplayAbility（抽象，InstancedPerActor；蒙太奇模板 PlayAttackMontage/PlayMontage + 4 回调；
                     EndEventTag 提前结束（bStopWhenAbilityEnds=false；未配置默认 Event.Combat.AttackEnd——
                     2026-08-29 ini 预注册 + 激活时惰性解析，防 CDO 时序坑）；FindNearestEnemy(半径, 状态)；
                     组合交接 TrySetupComboHandoff（opt-in，2026-08-29：配 NextComboAbility 时由 WaitCombo
                     驱动，先激活 Next 再 EndAbility，含切换退场守卫 IsSwitchingOut 早退））
├── UZZZBasicAttack      # 连段编排：PlayMontageAndWait + WaitInputBuffer + 基类组合交接 [+RotateToTarget]
│                        # （WaitCombo/过渡 2026-08-29 上移基类 opt-in；无条件调用，终端段保留关窗 flush）
├── UZZZEnemyAttack      # 敌人攻击：AbilityTags=Ability.Attack.Enemy，ActivationOwnedTags=State.Attacking
├── UZZZDodge            # 方向选蒙太奇（前/后），无敌=ActivationOwnedTags=State.Invulnerable，二连闪 CD
├── UZZZFollowUpAttack   # 闪避窗口派生攻击（单发模板）：Commit→RotateToTarget→PlayAttackMontage→完成即结束
│                        # 可选链出（2026-08-29）：配 NextComboAbility（如 GA_DashAttack→GA_BasicAttack_02）
│                        #   即收刀段 CanCombo 窗口内按攻击直接进下一段；未配 = 单发（不 spawn 组合任务）
│                        # 门控全在 BP 数据：GA_DashAttack（Trigger=Input.Attack + Required=CanDashAttack
│                        #   + Blocked=PerfectDodge）/ GA_DodgeCounter（额外 Required=PerfectDodge）
├── UZZZAssistDefensive  # 弹刀：入场全程无敌，Cancel 敌人攻击 + 敌人硬直（Phase 3 待做）
├── UZZZAssistOffensive  # 突击：入场前段小无敌 + 追击命中（Phase 3 待做）
├── UZZZSpecialAttack    # 特殊技 ✅ 2026-09-03（详见 §4.13）：普通=AttackMontage /
│                        #   强化=EnhancedMontage（能量≥EnergyCost 扣费——分支判定在 GA 内）;
│                        #   快速派生=TriggerEventData 带 SpecialQuickEntry → QuickStrike section;
│                        #   Asset Tags={Ability.Attack.Basic, Ability.Attack.Special};
│                        #   蒙太奇全程属 GA（无窗口/无 EndEventTag notify）
├── UZZZUltimate         # 终结技（全队 Decibel）——Phase 5
└── UZZZChainAttack      # 连携技（Director 调度）——Phase 5
```

每个角色的每个技能 = 上面类的 BP 资产（GA_Okuma_Attack_01~04 / GA_Okuma_Dodge…），差异（蒙太奇/伤害/判定/特效）全在资产层配置。

### 3.3 AttributeSet（UZZZAttributeSet）

Health/MaxHealth、Daze/MaxDaze、Attack/Defense、IncomingDamage（Meta，PostGameplayEffectExecute 消费归零）、TimeDilation（桥接 CustomTimeDilation）。**Energy/MaxEnergy（2026-09-03 落地**，特殊技资源，clamp [0, Max]）。访问器由 `ATTRIBUTE_ACCESSORS` 宏生成（`GetHealthAttribute()` 等）。AnomalyMastery/AnomalyProficiency/AnomalyBuildup 仍推迟。

### 3.4 GameplayTag 注册表

已注册（`Source/ZZZCombatRemake/ZZZ/Tags/ZZZGameplayTags.h`）：

```
Input:        Input.Attack / Input.Dodge / Input.Switch.Next / Input.Switch.Prev
              / Input.Special（特殊技 Y，2026-09-03）
Ability:      Ability.Attack.Basic(.BasicAttack01~04) / Ability.Attack.Enemy
              / Ability.Attack.Special（特殊技身份，2026-09-03）
              Ability.Defense.Dodge(.Perfect) / Ability.Defense.Assist / Ability.Switch.Quick
State:        State.Alive / Dead / Combat.Recovery / Stun / Staggered / Invulnerable
              / SlowMotion / Enemy / Player / Attacking / PerfectDodge / PassThrough
Event:        Event.Combat.Hit / Elimination / Stun / DodgePerfect / DodgeEnd / AttackEnd
              / DodgeSlowStart / Event.Combat.SpecialQuickEntry（特殊技快速派生入口，2026-09-03）
Effect:       Effect.Ability.CanCombo / Effect.Input.CanBuffer / Effect.Enemy.AttackWindow
              / Effect.Ability.CanDashAttack（CanDodge / CanParry 已废弃：判定前移 + 弹刀共用
              Enemy.AttackWindow，勿用）
Data:         Data.Damage / Data.Daze / Data.Energy（2026-09-03——⚠ 另需
              DefaultGameplayTags.ini 预注册：能量 GE CDO 的 FSetByCallerFloat.DataTag
              构造期解析，缺失 = 静默 0）
GameplayCue:  GameplayCue.ZZZ.DamageNumber / CameraShake(.Low/.Mid/.High) / EnemyAttackWarning
```

规划中（Phase 4/5）：Input.Ultimate、Ability.Attack.Ultimate、Ability.ChainAttack、Element.*、Anomaly.*、Team.Slot.*、State.SuperArmor/IgnoreInput、Event.Combat.ChainReady。

Tag 生命周期规则（2026-08-29 定稿，五层）：**A 动画窗口** → notify 配对 LooseTag + 双轨兜底（有主段 EndAbility / 无主段消费方显式清理）；**B 持久身份** → Infinite GE + 应用时 `DynamicGrantedTags`；**C 核心状态** → GE 管理（禁 LooseTag）；**D 能力生命周期** → `ActivationOwnedTags`；**E 本地路由标志** → Duration GE 自过期。**玩法 tag 除 A 层 notify 配对外禁用 `AddLooseGameplayTag`**；细则见 CLAUDE.md 硬性规则 1。

## 四、关键技术决策

### 4.1 输入链路（Phase 1.5 ✅）

IA_Attack → IMC → `UZZZInputConfig`(IA→Tag) → `AZZZCharacter::Input_AbilityInputTagPressed(Tag)`：
1. `ASC->HandleGameplayEvent(Tag)`（供 Task 监听）
2. 起手攻击（`Input.Attack`）：`TryActivateAbilityByClass(DefaultAbilities[0])`；其余输入由 AbilityTriggers/门控处理

⚠ 多角色泛化缺口：起手攻击硬编码 `DefaultAbilities[0]`，多角色时应改为按 Tag 匹配激活。`Input_AbilityInputTagReleased` 目前是 stub（蓄力需求时实现）。

### 4.2 AnimNotifyState 与 AbilityTask 分工

| 层 | 机制 |
|---|---|
| 输入缓冲/连段 | AbilityTask（WaitInputBuffer / WaitCombo，基类 opt-in 组合交接） |
| 动画窗口标记 | AnimNotifyState_AbilityWindow（管理 Tag：Begin 添加 / End 移除 / EndAbility 兜底） |
| 穿敌（胶囊体对通道 Block→Overlap）+ 停止索敌转向 | AnimNotifyState_CollisionPassThrough（per-instance：AffectedChannel 默认 Pawn / PassThroughResponse / WindowTag=State.PassThrough——`UAbilityTask_RotateToTarget` tick 查 tag 跳过转向，根运动驱动面向；伤害走 AttackTrace 球体检测不依赖 Overlap，穿透不影响判定；per-owner 捕获恢复） |
| 动画驱动旋转（临时关 CMC 自动转向） | AnimNotifyState_RotationOverride（per-owner 捕获/恢复 bOrientRotationToMovement + 可选 bUseControllerRotationYaw——玩家基线 Orient=true 是动画旋转被弹回的元凶） |
| 碰撞检测 + 打击帧反馈（卡肉/震屏 per-instance 开关） | AnimNotify_AttackTrace（AbilityTask_DoTrace 迁移推迟 Phase 5+） |

⚠ 两新 notify state 与窗口 tag 同族坑：蒙太奇被打断可能跳过 NotifyEnd → 状态残留（穿敌的碰撞/tag 一起残留，状态一致）；窗口保持短促，需硬保证时加能力侧恢复。

### 4.3 输入缓冲与连段（Phase 2 ✅，手感核心）

- **死区**：蒙太奇伤害帧后由 `AnimNotify_SendGameplayEvent(Event.AnimNotify.BeginInputBuffer)` 开启监听（纯动画侧可调）。
- **缓冲**：`UAbilityTask_WaitInputBuffer` 在 `Effect.Input.CanBuffer`（InputWindow）内把输入写入 **PC::BufferedInput**（不写 ASC LooseTag，防残留）。
- **连段窗口**：Recovery 段 `AnimNotifyState_AbilityWindow` 管理 `Effect.Ability.CanCombo`；`UAbilityTask_WaitCombo` 事件驱动（RegisterGameplayTagEvent + GenericGameplayEventCallbacks）检查窗口+缓冲，窗口关闭时清 PC 缓冲。
- **过渡顺序（关键，2026-08-29 上移基类 opt-in）**：基类 `TrySetupComboHandoff()` 创建 WaitCombo → 触发走 `OnComboHandoffTriggered()`（含切换退场守卫 `IsSwitchingOut()` 早退，2026-08-31 自 BasicAttack 迁入）——先 `TryActivateAbilityByClass(NextComboAbility)` 再 `EndAbility(当前)`，新 Montage BlendIn 覆盖收刀，避免空窗帧。BasicAttack 无条件调用（终端段保留关窗 flush 职责）；FollowUpAttack 等攻击族仅配置 `NextComboAbility` 时调用（如 GA_DashAttack → GA_BasicAttack_02，收刀段须另挂 CanCombo 窗口）。
- **移动打断**：`State.Combat.Recovery` 存在时 `Move()` → `CancelAbilities(Ability.Attack.Basic)` + `StopAnimMontage()`。

### 4.4 伤害统一 ExecCalc（UZZZDamageExecution）

- 捕获：Source.Attack / Target.Defense / Target.IncomingDamage / Target.Daze（Anomaly 捕获 Phase 4 再加）。
- SetByCaller（FGameplayTag 版）：`Data.Damage`（缺省回退 Attack 捕获）、`Data.Daze`（绝对值）。
- 公式：`DefenseFactor = Max(0, 1 - Defense/(Defense+500))`；`FinalDamage = Max(0, BaseDamage × DefenseFactor)`。
- 输出：Target.IncomingDamage（Meta，消费转扣血）+ Target.Daze（叠加，满阈值 → State.Stun）。
- API 事实：`FGameplayModifierEvaluatedData` 构造必须 4 参（第 4 参 `FActiveGameplayEffectHandle` 传空）；`Execute_Implementation` 为 const。

### 4.5 时间管理（Phase 3.5，CustomTimeDilation 桥接）

- `CustomTimeDilation` 是 AActor 原生属性，GE 不能直接改 → AttributeSet 加 `TimeDilation` 桥接属性；**Duration/Infinite GE 的属性变化不触发 PostGameplayEffectExecute，必须走 `GetGameplayAttributeValueChangeDelegate()`**（M1）。
- 5.8 API：`SetCustomTimeDilation` 已移除，直接赋 `CustomTimeDilation` 属性。
- `GE_SlowMotion`：只减速 `State.Enemy`（Tag 过滤），玩家（State.Player）豁免；`GE_PlayerSlowMotion` 减速玩家。⚠ **GE 时长走世界时间**：每个 Duration GE 在世界 FTimerManager 注册定时器（`FActiveGameplayEffect::DurationHandle` → `UAbilitySystemComponent::CheckDurationExpired`），角色 CustomTimeDilation 只缩放 actor/组件 tick，**不影响 GE 时长/冷却**——Duration 直接设真实秒数（2026-08-15 引擎源码查证）。时长定调（2026-08-16）：敌人 ~1s、玩家与 CanDashAttack 窗口同步（BP 资产按手感调）；玩家慢放 = **决策窗口**，反击激活即被 `UZZZFollowUpAttack` 移除（`State.SlowMotion` tag 配在 `GE_PlayerSlowMotion` 的 TargetTags 组件上）。
- Hit Stop：`UZZZGameplayEffect_HitStop`（C++ 载体，Duration 0.03s 世界时间 + TimeDilation Override 0.01——LOW 档默认，重击换 BP 子类）由 `ZZZAnimNotify_AttackTrace` 在命中帧对攻击者与受击者双向施加；到期后 aggregator 重算、桥接自动恢复（慢放进行中则回慢放值）。notify 每实例可开关/换资产（`bApplyHitStop`/`HitStopEffect`、`bApplyCameraShake`/`CameraShakeCueTag`——Low/Mid/High 三档 tag，默认 Low，各挂一个 `GC_ZZZ_CameraShake` BP 子处理器，共用单一 `BP_HitShake` shake 资产：震屏是本地玩家相机行为，不再区分 Player/Enemy 资产，一个 tag 只挂一个处理器）；空挥与无敌/已死目标不触发（pre-hit 快照，击杀帧保留反馈）。桥接的组件抽取推迟至第三个 Actor 类出现。

### 4.6 消灭 / 失衡反馈（Elimination）

- HP ≤ 0 → `Event.Combat.Elimination`（命名禁用 Kill）；Daze 满 → `Event.Combat.Stun` + Daze 清零。
- 热路径 tag 用 `UE_DEFINE_GAMEPLAY_TAG_STATIC` 静态初始化，避免字符串查找。
- 特效/震屏/飘字一律 GameplayCue（`GC_ZZZ_*`），不在 PostGameplayEffectExecute 直接 SpawnEmitter。

### 4.7 收刀动画与连段窗口（Phase 2 ✅）

- 每段攻击 = 一个 Montage（Attack + Recovery 两 Section）+ 一个 GA；连段 = Ability 切换，非 Section 跳转。
- 收刀 = 合法输入窗口：Recovery 段嵌 AbilityWindow（CanCombo）；Root Motion 全程开启（收刀段不关，防 capsule 与骨骼脱节）；BlendIn 建议 0.05-0.1s。
- 两段式统一机制（2026-08-08）：基类 `EndEventTag`（GA_Dodge=Event.Combat.DodgeEnd、GA_BasicAttack_N=Event.Combat.AttackEnd）——蒙太奇动作段末尾挂 AnimNotify 发事件 → GA 提前 EndAbility，过渡段无主播放（`bStopWhenAbilityEnds=false`）。连段窗口关闭后 GA 即结束，门控放行下次起手。**未配置默认 `Event.Combat.AttackEnd`**（2026-08-29）：基类惰性解析（`GetEndEventTag()`，激活/结束时有效）+ `Config/DefaultGameplayTags.ini` 预注册（`+GameplayTagList=`）——CDO 构造早于 native tag 注册，裸 `RequestGameplayTag` 在构造函数会 ensure（规则 2），ini 为唯一 CDO 期可用来源。
- **收刀打断标准流程（2026-08-29 定稿，新技能一律照此）**：① 动作段末 notify（`EndEventTag`）决定 GA 结束位置 → ② 收刀段无主播放 + `AbilityWindow(State.Combat.Recovery)` 可打断 tag → ③ 打断入口（`Move()`，`ZZZCharacter.cpp:236`）查询 tag → StopAnimMontage + **消费方显式 `RemoveLooseGameplayTag(State.Combat.Recovery)`**（无主段 EndAbility 兜底不可达——GA 已结束；停蒙太奇可能跳过 NotifyEnd，2026-08-29 修复；对应 CLAUDE.md 规则 1 A 层双轨②）。**不调用 CancelAbilities**（2026-08-29 定稿）：GA 由蒙太奇中断回调 OnInterrupted → EndAbility 覆盖（与闪避被追击技打断同链）；现 `Move()` 中的 Cancel 为旧方式残留，现有 basic attack 依赖暂保留，新技能不依赖。

### 4.8 闪避与完美闪避（Phase 3 进行中）

- 输入：`Input.Dodge`（`bTriggerOnStarted=true`，Started 语义）。
- `UZZZDodge`：Commit → Cancel 普攻 → 按 `LastInputVector` 选前/后蒙太奇（方向是输入数据非能力身份）；无敌 = `ActivationOwnedTags=State.Invulnerable`；二连闪 CD（DoubleDodgeWindow 0.7s → DodgeCooldown 0.7s）；程序化位移兜底（`bUseProceduralDisplacement`，DodgeAcceleration=11000 cm/s² + 0.22s ≈ 266cm；Root Motion 动画优先）。闪避全程可攻击（不配 BlockAbilitiesWithTag）。
- 完美窗口 = 蒙太奇前段 AbilityWindow（`Effect.Ability.CanDodge`）；完美闪避：IncomingDamage 分支拦截（Invulnerable + CanDodge）→ 敌人 GE_SlowMotion + 玩家 GE_PlayerSlowMotion（决策窗口；**不震屏**——慢放本身就是奖励，2026-08-16）。
- 冲刺攻击/闪避反击（✅ 珂蕾妲资产完成、流程跑通，2026-09-02）：`State.PerfectDodge` 改 Duration GE（GE_PerfectDodge_Status，0.5s）；两者同为 `UZZZFollowUpAttack`（见 §3.2）的 BP 子类，差异全在数据——GA_DashAttack（Trigger=Input.Attack + Required=CanDashAttack + Blocked=PerfectDodge）、GA_DodgeCounter（额外 Required=PerfectDodge）。⚠ 起手守卫（闪避中且 CanDashAttack → 跳过普攻起手）**必须在 `HandleGameplayEvent` 之前判定**：冲刺攻击激活即打断闪避蒙太奇 → 闪避 EndAbility 移除 CanDashAttack 窗口 tag（兜底清理），事后判定会看到死窗口而误放普攻覆盖冲刺攻击（2026-08-11 修复）。

### 4.9 弹刀 / 突击 / 编队切换（普通切换 ✅ 2026-09-02 落地；弹刀/突击待做）

#### 4.9.1 普通切换（手动切人）——已实现

- **输入**：PC 直绑 `SwitchAction`（`ETriggerEvent::Started` 防连切，非 InputConfig tag 路由）；候选 = `SquadClasses` 轮转、跳过当前职业与阵亡职业（当前存活时）；**`bIsSwitching` 守卫**——旧人物完全隐藏前屏蔽再次切换（未配置/无候选 → 屏幕提示）。
- **新人物立即进场**（不等旧人物）：`GetOrSpawnSquadMember`（持久成员：首次生成、隐藏不销毁、ASC 继续 Tick）→ 对齐位置+朝向 → **`BeginSwitchIn()`**（复位材质透明度 → 显示+开碰撞 → 播 `EnterMontage`，可空）——新旧两人短暂同场。
- **入场位置 = 旧人物右后方**（入场动画是前冲演出）：`-旧Forward×SwitchInOffset + 旧Right×SwitchInRightOffset`（PC 资产 `ZZZ|Squad`，默认 2000/250——`SwitchInOffset` ≈ 入场动画前冲位移，动画冲完恰好到位）；朝向继承旧人物，前冲沿旧 facing 方向。
- **旧人物异步退场状态机 `StartSwitchOut()`**（AZZZCharacter 侧；PC 编排，等 `OnSwitchOutCompleted` 清守卫）：
  1. 挂切换无敌（`State.Invulnerable`，GE 管理——旧人物退场全程站场可被打死）→ 清窗口 tag（CanCombo/CanBuffer）
  2. 等待判定：`SwitchWaitAbilityTags`（角色 BP，空 → 运行时默认 `Ability.Attack.Basic`，资产 tag 层级匹配 GA_01..04）任一活动 → 绑 `ASC->OnAbilityEnded` + `State.Dead` tag 安全阀，等其 **EndAbility**（**GA 结束判定，非蒙太奇结束**；取消/打断同样触发）
  3. 退场动画按 `bSwitchWaitedForAbility`（本次是否等待过攻击 GA）选择：攻击中切换 → **GA 衔接动画 `ExitMontage`**（如 AM_SwitchOut_InAttack）；无攻击（跑步/idle）→ **`RunningExitMontage`**。不用速度判定（技能带位移，速度不可靠）；槽为空回落另一个，全空 → 直接淡出
  4. **淡出与动画并行**：动画第 1 帧即启动材质淡出，**隐藏时刻由 `FadeDuration` 直接控制**（= 动画开始 → 隐藏总时长；与动画时长对齐则播完恰好隐藏）——动画结束回调不介入隐藏
  5. 材质淡出 = 逐材质槽 `CreateDynamicMaterialInstance` + 定时器驱动标量参数（默认 `Opacity`）1→0；材质侧：**Blend Mode = Masked + 裁切链路**（OpacityMask ← 参数；要平滑溶解加 `DitherTemporalAA`）。⚠ **MI 的 Material Property Overrides 若覆写 BlendMode=Opaque 会盖掉父级 Masked**（FBX 导入材质自带，换淡出父材质后必须清除——2026-09-01 排障记录）
  6. 淡出归零 → `FinalizeSwitchOut`：`CancelAllAbilities` 兜底 → 清切换无敌 → 隐藏+关碰撞 → 广播 `OnSwitchOutCompleted` → PC 清守卫
  7. 死亡（`State.Dead`）任意阶段 → 立即 Finalize（跳过动画/淡出）
- **竞态双守卫**：旧角色残段蒙太奇期间其 WaitCombo 仍随 ASC tick，而 PC 共享输入缓冲已归新角色——`UZZZBasicAttack::CheckComboTransition` 与 `AbilityTask_WaitCombo::OnComboWindowChanged` 顶部查 `IsSwitchingOut()` 早退：防旧角色连段 + 防关窗 flush 吞新角色攻击缓冲。
- **相机**：`Possess` 前后保存/恢复 ControlRotation（防镜头被拉向角色朝向、俯仰清零）；view target 由 `OnPossess` 统一设置（GameplayCameras **manager** Push 模式，见 `Docs/ZZZ-Camera-Architecture.md` §三）→ CA_PlayerCameras EnterTransitions 过渡。
- **配置槽位**：角色 BP `ZZZ|Switch`（`EnterMontage` / `ExitMontage` / `RunningExitMontage` / `FadeParameterName`=Opacity / `FadeDuration` / `SwitchWaitAbilityTags`）；PC 资产 `ZZZ|Squad`（`SquadClasses` / `SwitchInOffset` / `SwitchInRightOffset`）。
- **降级路径**：无退场动画 → 直接淡出；材质无淡出参数/非 Masked → 淡出无视觉效果但时序照常（FadeDuration 后隐藏）。

#### 4.9.2 弹刀 / 突击（Phase 3 待做，2026-08-09 设计定稿）

- 切换键自动判定（状态优先级语义）：`FindNearestEnemy(300, Effect.Enemy.AttackWindow)` → 弹刀；`FindNearestEnemy(600, State.Staggered)` → 突击；否则普通切换。弹刀窗口与极限闪避共用 `Effect.Enemy.AttackWindow`（黄闪同步段）；攻击已出手/收招段切人 = 普通切换。
- **弹刀原型取舍**：无精防判定窗口、无资源消耗（可零成本反复触发）——精防与支援点经济推迟 Phase 5。交付"无条件格挡换人 + 敌人硬直惩罚"。
- `UZZZAssistDefensive`：ActivationOwnedTags=State.Invulnerable（入场全程）→ Cancel 敌人攻击（`Ability.Attack.Enemy`）→ 移除 AttackWindow 兜底 → 敌人挂 `State.Staggered`（UZZZGameplayEffect_Stagger，Duration 0.35s）→ 入场蒙太奇（前段 CanParry + 中后段 AttackTrace 反击）。
- `UZZZAssistOffensive`：入场前段小无敌（AbilityWindow=State.Invulnerable，非全程）+ RotateToTarget + 命中。
- **旧人物下台统一复用 §4.9.1 的退场状态机**（`StartSwitchOut`：等当前 GA 结束 → 退场动画 → 淡出 → 隐藏）；入场方播 Assist **专属**动画（各自 GA 的 `AttackMontage` 槽位），与手动切换的 `EnterMontage` 互不共享（弹刀/连携是独立演出资产，2026-09-02 确认）。能力激活失败（Commit 不过/tag 阻塞）回落普通切换表现。
- 成员持久化（隐藏/禁碰撞/不销毁），ASC 继续 Tick；切换不 Cancel 旧能力。

### 4.10 连携技 Director（Phase 5）

目标进入 `State.Stun` → `AZZZCombatDirector`（GameState 子组件）调度：暂停敌人 AI（SlowMotion）+ 非活跃队友 IgnoreInput/Invulnerable → 连携选择 UI → `TryActivateAbilitiesByTag(Ability.ChainAttack)` → `AbilityTask_ChainCamera` 镜头 Lerp → 最多 3 次，已行动角色不可重复。

### 4.11 元素异常模型（Phase 4）

ExecCalc 统一计算 AnomalyBuildup → 目标施加对应 GE（Infinite+Stack）→ 达阈值触发异常爆发 GE（灼烧/冻结/感电/强击/侵蚀）→ FIFO 单异常 → 清空积蓄。属性变化走 Delegate（非 PostGameplayEffectExecute）。

### 4.12 敌人攻击预警黄闪（✅ 2026-08-08）

- 链路：蒙太奇抬手帧 → 引擎内置 "GameplayCue (Burst)" Notify（零 C++）→ `GameplayCue.ZZZ.EnemyAttackWarning` → `UGC_ZZZ_EnemyAttackWarning`（Static，Niagara 自灭，挂 `hand_r` socket，Emitter 勾 Local Space 跟随拳头；消除检查 State.Dead）。
- 注册表镜像坑（R20）：GCN 必须重写 `PostInitProperties`/`PostLoad`/`Serialize` 同步 `GameplayCueName = GameplayCueTag.GetTagName()` + `IsOverride=true` + ini 注册（`+Prop=` 语法），否则 CueManager 扫描 unmapped 静默丢弃；资产重存需先标 dirty。
- 扩展：红闪（不可闪避）/蓝闪（弹刀）同一 GCN 换色或按敌人子类配不同 cue tag。

### 4.13 特殊技 + 能量系统（✅ C++ 2026-09-03；资产待做）

**触发与门控**（Y 键，无 CD，无缓冲）：
- 可触发 = **各技能可输入窗口**（`Effect.Ability.CanCombo` / `Effect.Ability.CanDashAttack` / `State.Combat.Recovery` 任一在身）**+ 自由态**（无任何玩家战斗 GA 活动）。
- 拒绝 = 切换退场中 / 已阵亡 / 特殊技自身活动中（防自链）/（**忙 且 无窗口**）——忙用**类扫描**（活动 spec 的 Ability 是 `UZZZGameplayAbility` 子类），勿用 tag 枚举（GA_DashAttack 等资产 tag 是 BP 数据，代码不可证，漏判 = Y 顶掉进行中的技能）。
- 门控全在 `AZZZCharacter::TryActivateSpecialAttack()`（`Input_AbilityInputTagPressed` 的 Input.Special 分支）；GA **勿配 AbilityTriggers**（否则同帧双激活）。

**普通 vs 强化（单 GA 内运行时分支）**：同一 `UZZZSpecialAttack` BP 子类；`Energy >= EnergyCost`（默认 50，门槛==消耗，逐角色 GA 配）→ 播 `EnhancedMontage` 并扣 EnergyCost（手动 GE Apply——引擎 AbilityCosts 无法携带 per-instance SetByCaller 幅值）；不足 → 播基类 `AttackMontage`，不扣费。能量判定**留在 GA 内**（输入门控不预选版本）。

**快速派生**（段 2/4 衔接）：普攻第 2/4 段（角色 BP `ZZZ|Combat.QuickEntryComboIndexes`，空=禁用）连段窗口内按 Y → 跳过打击 1，从蒙太奇 `QuickStrike` section 起播。入口标签 `Event.Combat.SpecialQuickEntry` 随 Character 的 `InternalTryActivateAbility(Handle, ..., &EventData)` 直传（公开 `TryActivateAbility` 不带事件数据；引擎 `CallActivateAbility` 会把它送进 `ActivateAbility` 覆写）——**不在普攻蒙太奇加 notify**。特殊技动作 = 一次 Y 打完两击（同蒙太奇两伤害 notify）；普通/强化蒙太奇各含 `QuickStrike` section（起点=打击 2 预备帧，对齐普攻段收招姿态），4 情形（普通/强化 × 完整/快速）由 1 类 + 2 资产覆盖。播放走基类 `PlayMontage(Montage, StartSection)`（2026-09-03 基类加可选 StartSection 透传，加法改动）。

**蒙太奇/GA 生命周期（结构 A）**：整条蒙太奇属 GA——无窗口 notify、无 AttackEnd notify（误放会导致 GA 提前结束、收尾变无主段）；蒙太奇播完 = 基类回调 EndAbility；收尾后自由态，攻击从普攻第 1 段重起。EndAbility 兜底清 `CanCombo/CanBuffer/Recovery`（防御误拷 notify 布局）。激活照 `UZZZFollowUpAttack` 模板清 `State.SlowMotion`（完美闪避慢放决策窗结束，防 0.5x 全程）。

**资产双 tag 副作用（有意为之）**：GA Asset Tags = {`Ability.Attack.Basic`, `Ability.Attack.Special`} → ① 普攻起手抑制 `!IsActive(Basic)` 自动覆盖（特殊技中按攻击不顶掉它）② 切人默认等待 tag Basic → 特殊技期间切人自动等播完 ③ Dodge 的 `CancelAbilities(Basic)` → 闪避可取消特殊技（特性，与既有约定一致）。

**能量属性/GE**：`UZZZAttributeSet::Energy/MaxEnergy`（默认 0/100，PreAttributeChange clamp）。**所有能量变化走 `UZZZGameplayEffect_EnergyDelta`**（C++ Instant 载体，Additive modifier on Energy，幅值 SetByCaller `Data.Energy`）——直改 `SetEnergy()` 绕过聚合器，未来能量条绑 ValueChangeDelegate 收不到。⚠ 5.8 引擎修正：`FGameplayEffectModifierMagnitude` 成员已 protected，SetByCaller 幅值只能 ctor 传 `FSetByCallerFloat`；`Data.Energy` 依赖 ini 预注册（CDO 构造期解析，缺失静默 0）。
- **命中回能**（每次命中固定值，默认 +2，逐角色 `EnergyGainPerHit`）：挂 target 侧 AttributeSet 伤害确认单点（IncomingDamage 分支、Hit 事件后/死亡判定前）——条件 instigator 是 `AZZZCharacter` 且目标带 `State.Enemy`；击杀帧给、被无敌吸收帧不给（吸收已提前 return）。
- **自然回能**（默认 1/s，`EnergyRegenPerSecond` × 0.2s tick）：角色世界 FTimer 循环（`StartEnergyRegen`，InitAbilitySystem 末尾启动），世界时间不受 HitStop/慢放拉伸；隐藏队员照常回。
- 能量在切换中保留（每成员独立 AttributeSet，与战损同语义）；`InitialEnergy`（默认 0，PIE 调参可临时抬高）。

## 五、实施路线图

| Phase | 内容 | 状态 |
|---|---|---|
| 1 | GAS 基础设施（AttributeSet/基类/ExecCalc/Tags/角色） | ✅ |
| 1.5 | 输入→Tag 桥接（UZZZInputConfig） | ✅ |
| 2 | 普攻连段 + 双窗口缓冲 + 伤害管线 + 飘字 | ✅（遗留：AbilityTask_DoTrace 推迟 Phase 5+） |
| 3 | 闪避/完美闪避/弹刀/突击/编队切换/特殊技+能量 | 🔄 进行中（特殊技+能量 C++ ✅ 2026-09-03，见 Phase3 计划） |
| 3.5 | 时间管理（SlowMotion/HitStop/震屏） | 🔄 并入 Phase 3 |
| 4 | 元素 & 异常 | ⬜ |
| 5 | 终结技 & 连携技（Director + Decibel + ChainCamera + 相机混合） | ⬜ |
| 6 | AI & 关卡（StateTree 升级/波次/HUD 合成） | ⬜ |

## 六、数据驱动设计

- `UZZZCharacterData`（PrimaryDataAsset）：BaseHP/BaseAttack/BaseDefense/BaseEnergy/AnomalyMastery/AnomalyProficiency/ElementType/GrantedAbilities——**待落地**，替换 AttributeSet 构造函数硬编码默认值。
- 技能参数 DataTable（Damage/EnergyCost/DazeBuildup/AnomalyBuildup/ComboWindow），可选。

## 七、实现要点

1. Root Motion：所有攻击/闪避蒙太奇开启 Root Motion from Montages，收刀段不关。
2. SetByCaller 一律 FGameplayTag 版（`GetSetByCallerMagnitude(tag, false, default)`）。
3. 视觉反馈：`ExecuteGameplayCueOnActor`，GameplayCue 命名 `GC_ZZZ_*`。
4. M1 回调区分：Instant → PostGameplayEffectExecute；Duration/Infinite → ValueChangeDelegate。
5. UI 事件驱动：绑定 `GetGameplayAttributeValueChangeDelegate`，不轮询。
6. 敌人统一 ASC on Pawn；小怪/Boss 差异在配置层（数值/能力数）。
7. 5.8 坑：`FGameplayModifierEvaluatedData` 4 参；`SetCustomTimeDilation` 移除；UFUNCTION 参数禁止 struct 裸指针（用 GenericGameplayEventCallbacks + lambda）；GCN GameplayCueName 镜像（4.12）。

## 八、术语映射

| ZZZ 术语 | 本项目类/概念 |
|---|---|
| 普攻 | `UZZZBasicAttack` → `GA_BasicAttack_0X` |
| 特殊技 / 强化特殊技 (EX) | `UZZZSpecialAttack` → `GA_SpecialAttack_*`（✅ 2026-09-03，能量分支在 GA 内；强化版能量≥EnergyCost 并扣费） |
| 能量 | `UZZZAttributeSet::Energy/MaxEnergy` + `UZZZGameplayEffect_EnergyDelta`（SetByCaller Data.Energy） |
| 终结技 | `UZZZUltimate` → `GA_Ultimate`（消耗 Decibel） |
| 闪避 / 完美闪避 | `UZZZDodge` + `State.SlowMotion` / `Ability.Defense.Dodge.Perfect` |
| 弹刀 / 招架 | `UZZZAssistDefensive` → `GA_AssistDefensive` |
| 突击支援 | `UZZZAssistOffensive` → `GA_AssistOffensive` |
| 连携技 | `UZZZChainAttack` → `GA_ChainAttack`（Director 调度） |
| 架势 / 失衡 | `UZZZAttributeSet::Daze` / `MaxDaze` |
| 异常积蓄 / 爆发 | `GE_AnomalyBuildup_*` / `GE_Anomaly_*` |
| 以骸 | `AZZZCombatEnemy` 子类 |
| 代理人 | `AZZZCharacter` 子类 |
| Decibel | 终极技能量（全队共享） |

## 九、遗留事项

- `GC_ZZZ_DamageNumber` 补 R20 `GameplayCueName` 重写（防重存复发）
- 多角色输入路由泛化（起手硬编码 `DefaultAbilities[0]` → 按 Tag）
- `UZZZCharacterData` 落地
- `AbilityTask_DoTrace` 迁移
- 模块拆分（按需）
- Phase 4/5/6（见路线图）
