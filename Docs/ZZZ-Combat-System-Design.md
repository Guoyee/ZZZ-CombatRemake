# 绝区零核心战斗逻辑复刻 — 架构设计文档

> **项目**: ZZZCombatRemake (UE 5.8) · 单机 · DX12/SM6
> **目标**: 在现有 ThirdPerson 基础上复刻《绝区零》核心战斗逻辑
> **状态**: Phase 1/1.5/2 完成，Phase 3 进行中
> **旧版备份**: `Docs/archive/ZZZ-Combat-System-Design.md.orig`（勿读，仅查证历史）

## 一、目标系统

| 系统 | 说明 | 状态 |
|---|---|---|
| 基础攻击连段 | 3-5 段普攻，预输入缓冲 | ✅ Phase 2（4 段） |
| 特殊攻击 EX | 消耗能量强化攻击 | ⬜ Phase 3+ |
| 闪避 / 完美闪避 | 极限时机触发时空断裂 | 🔄 Phase 3 |
| 弹刀 / 突击支援 | 防御/进攻型角色切换 | 🔄 Phase 3 |
| 终结技 | 消耗全队共享 Decibel | ⬜ Phase 5 |
| 连携技 Chain Attack | 打空 Daze 后多人连携 | ⬜ Phase 5 |
| 元素 / 异常 | Fire/Ice/Electric/Physical/Ether + 积蓄爆发 | ⬜ Phase 4 |
| Daze 失衡 | 攻击积累架势伤害，满后失衡 | ✅ 数值（连携 ⬜ Phase 5） |
| 三人编队切换 | 持久化成员，隐藏/显示 + Possess | 🔄 原型已验证 |
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

`AZZZCharacter` 职责：移动/相机、ASC 初始化（含阵营 GE `State.Player`）、`DefaultAbilities` 授予（按 class 去重）、输入绑定。切换只做物理切换（隐藏旧+显示新+Possess），不 Cancel 旧能力（脱手技语义）。

### 3.2 Ability

```
UZZZGameplayAbility（抽象，InstancedPerActor；蒙太奇模板 PlayAttackMontage/PlayMontage + 4 回调；
                     EndEventTag 提前结束（bStopWhenAbilityEnds=false）；FindNearestEnemy(半径, 状态)）
├── UZZZBasicAttack      # 连段编排：PlayMontageAndWait + WaitInputBuffer + WaitCombo [+RotateToTarget]
│                        # 连段过渡：先 TryActivateAbilityByClass(Next) 再 EndAbility(当前)
├── UZZZEnemyAttack      # 敌人攻击：AbilityTags=Ability.Attack.Enemy，ActivationOwnedTags=State.Attacking
├── UZZZDodge            # 方向选蒙太奇（前/后），无敌=ActivationOwnedTags=State.Invulnerable，二连闪 CD
├── UZZZFollowUpAttack   # 闪避窗口派生攻击（单发模板）：Commit→RotateToTarget→PlayAttackMontage→完成即结束
│                        # 门控全在 BP 数据：GA_DashAttack（Trigger=Input.Attack + Required=CanDashAttack
│                        #   + Blocked=PerfectDodge）/ GA_DodgeCounter（额外 Required=PerfectDodge）
├── UZZZAssistDefensive  # 弹刀：入场全程无敌，Cancel 敌人攻击 + 敌人硬直（Phase 3 待做）
├── UZZZAssistOffensive  # 突击：入场前段小无敌 + 追击命中（Phase 3 待做）
├── UZZZSpecialAttack    # EX 特攻（能量门槛/消耗）——Phase 3+
├── UZZZUltimate         # 终结技（全队 Decibel）——Phase 5
└── UZZZChainAttack      # 连携技（Director 调度）——Phase 5
```

每个角色的每个技能 = 上面类的 BP 资产（GA_Okuma_Attack_01~04 / GA_Okuma_Dodge…），差异（蒙太奇/伤害/判定/特效）全在资产层配置。

### 3.3 AttributeSet（UZZZAttributeSet）

Health/MaxHealth、Energy/MaxEnergy、Daze/MaxDaze、Attack/Defense、AnomalyMastery/AnomalyProficiency/AnomalyBuildup、IncomingDamage（Meta，PostGameplayEffectExecute 消费归零）、TimeDilation（桥接 CustomTimeDilation）。访问器由 `ATTRIBUTE_ACCESSORS` 宏生成（`GetHealthAttribute()` 等）。

### 3.4 GameplayTag 注册表

已注册（`Source/ZZZCombatRemake/ZZZ/Tags/ZZZGameplayTags.h`）：

```
Input:        Input.Attack / Input.Dodge / Input.Switch.Next / Input.Switch.Prev
Ability:      Ability.Attack.Basic(.BasicAttack01~04) / Ability.Attack.Enemy
              Ability.Defense.Dodge(.Perfect) / Ability.Defense.Assist / Ability.Switch.Quick
State:        State.Alive / Dead / Combat.Recovery / Stun / Staggered / Invulnerable
              / SlowMotion / Enemy / Player / Attacking / PerfectDodge / PassThrough
Event:        Event.Combat.Hit / Elimination / Stun / DodgePerfect / DodgeEnd / AttackEnd
              / Event.Combat.DodgeSlowStart
Effect:       Effect.Ability.CanCombo / Effect.Input.CanBuffer / Effect.Enemy.AttackWindow
              / Effect.Ability.CanDashAttack（CanDodge / CanParry 已废弃：判定前移 + 弹刀共用
              Enemy.AttackWindow，勿用）
Data:         Data.Damage / Data.Daze
GameplayCue:  GameplayCue.ZZZ.DamageNumber / CameraShake(.Low/.Mid/.High) / EnemyAttackWarning
```

规划中（Phase 4/5）：Input.Special/Ultimate、Ability.Attack.Special/Ultimate、Ability.ChainAttack、Element.*、Anomaly.*、Team.Slot.*、State.SuperArmor/IgnoreInput、Event.Combat.ChainReady。

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
| 输入缓冲/连段 | AbilityTask（WaitInputBuffer / WaitCombo） |
| 动画窗口标记 | AnimNotifyState_AbilityWindow（管理 Tag：Begin 添加 / End 移除 / EndAbility 兜底） |
| 碰撞检测 + 打击帧反馈（卡肉/震屏 per-instance 开关） | AnimNotify_AttackTrace（AbilityTask_DoTrace 迁移推迟 Phase 5+） |

### 4.3 输入缓冲与连段（Phase 2 ✅，手感核心）

- **死区**：蒙太奇伤害帧后由 `AnimNotify_SendGameplayEvent(Event.AnimNotify.BeginInputBuffer)` 开启监听（纯动画侧可调）。
- **缓冲**：`UAbilityTask_WaitInputBuffer` 在 `Effect.Input.CanBuffer`（InputWindow）内把输入写入 **PC::BufferedInput**（不写 ASC LooseTag，防残留）。
- **连段窗口**：Recovery 段 `AnimNotifyState_AbilityWindow` 管理 `Effect.Ability.CanCombo`；`UAbilityTask_WaitCombo` 事件驱动（RegisterGameplayTagEvent + GenericGameplayEventCallbacks）检查窗口+缓冲，窗口关闭时清 PC 缓冲。
- **过渡顺序（关键）**：`CheckComboTransition()` 先 `TryActivateAbilityByClass(NextComboAbility)`，再 `EndAbility(当前)`——新 Montage BlendIn 覆盖收刀，避免空窗帧。
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
- 两段式统一机制（2026-08-08）：基类 `EndEventTag`（GA_Dodge=Event.Combat.DodgeEnd、GA_BasicAttack_N=Event.Combat.AttackEnd）——蒙太奇动作段末尾挂 AnimNotify 发事件 → GA 提前 EndAbility，过渡段无主播放（`bStopWhenAbilityEnds=false`）。连段窗口关闭后 GA 即结束，门控放行下次起手。
- **收刀打断标准流程（2026-08-29 定稿，新技能一律照此）**：① 动作段末 notify（`EndEventTag`）决定 GA 结束位置 → ② 收刀段无主播放 + `AbilityWindow(State.Combat.Recovery)` 可打断 tag → ③ 打断入口（`Move()`，`ZZZCharacter.cpp:236`）查询 tag → StopAnimMontage + **消费方显式 `RemoveLooseGameplayTag(State.Combat.Recovery)`**（无主段 EndAbility 兜底不可达——GA 已结束；停蒙太奇可能跳过 NotifyEnd，2026-08-29 修复；对应 CLAUDE.md 规则 1 A 层双轨②）。**不调用 CancelAbilities**（2026-08-29 定稿）：GA 由蒙太奇中断回调 OnInterrupted → EndAbility 覆盖（与闪避被追击技打断同链）；现 `Move()` 中的 Cancel 为旧方式残留，现有 basic attack 依赖暂保留，新技能不依赖。

### 4.8 闪避与完美闪避（Phase 3 进行中）

- 输入：`Input.Dodge`（`bTriggerOnStarted=true`，Started 语义）。
- `UZZZDodge`：Commit → Cancel 普攻 → 按 `LastInputVector` 选前/后蒙太奇（方向是输入数据非能力身份）；无敌 = `ActivationOwnedTags=State.Invulnerable`；二连闪 CD（DoubleDodgeWindow 0.7s → DodgeCooldown 0.7s）；程序化位移兜底（`bUseProceduralDisplacement`，DodgeAcceleration=11000 cm/s² + 0.22s ≈ 266cm；Root Motion 动画优先）。闪避全程可攻击（不配 BlockAbilitiesWithTag）。
- 完美窗口 = 蒙太奇前段 AbilityWindow（`Effect.Ability.CanDodge`）；完美闪避：IncomingDamage 分支拦截（Invulnerable + CanDodge）→ 敌人 GE_SlowMotion + 玩家 GE_PlayerSlowMotion（决策窗口；**不震屏**——慢放本身就是奖励，2026-08-16）。
- 冲刺攻击/闪避反击（进行中）：`State.PerfectDodge` 改 Duration GE（GE_PerfectDodge_Status，0.5s）；两者同为 `UZZZFollowUpAttack`（见 §3.2）的 BP 子类，差异全在数据——GA_DashAttack（Trigger=Input.Attack + Required=CanDashAttack + Blocked=PerfectDodge）、GA_DodgeCounter（额外 Required=PerfectDodge）。⚠ 起手守卫（闪避中且 CanDashAttack → 跳过普攻起手）**必须在 `HandleGameplayEvent` 之前判定**：冲刺攻击激活即打断闪避蒙太奇 → 闪避 EndAbility 移除 CanDashAttack 窗口 tag（兜底清理），事后判定会看到死窗口而误放普攻覆盖冲刺攻击（2026-08-11 修复）。

### 4.9 弹刀 / 突击 / 编队切换（Phase 3 待做，2026-08-09 设计定稿）

- 切换键自动判定（状态优先级语义）：`FindNearestEnemy(300, Effect.Enemy.AttackWindow)` → 弹刀；`FindNearestEnemy(600, State.Staggered)` → 突击；否则普通切换。弹刀窗口与极限闪避共用 `Effect.Enemy.AttackWindow`（黄闪同步段）；攻击已出手/收招段切人 = 普通切换。
- **弹刀原型取舍**：无精防判定窗口、无资源消耗（可零成本反复触发）——精防与支援点经济推迟 Phase 5。交付"无条件格挡换人 + 敌人硬直惩罚"。
- `UZZZAssistDefensive`：ActivationOwnedTags=State.Invulnerable（入场全程）→ Cancel 敌人攻击（`Ability.Attack.Enemy`）→ 移除 AttackWindow 兜底 → 敌人挂 `State.Staggered`（UZZZGameplayEffect_Stagger，Duration 0.35s）→ 入场蒙太奇（前段 CanParry + 中后段 AttackTrace 反击）。
- `UZZZAssistOffensive`：入场前段小无敌（AbilityWindow=State.Invulnerable，非全程）+ RotateToTarget + 命中。
- **切换时序**：先 Possess 新成员 → 旧成员播 SwitchOut（挂 State.Invulnerable 防受击打断）→ 播完隐藏+关碰撞；Interrupted 分支也走隐藏兜底。`CancelSwitchOut()` 先清标志再 Montage_Stop。能力激活失败（Commit 不过/tag 阻塞）回落普通切换表现。
- 成员持久化（隐藏/禁碰撞/不销毁），ASC 继续 Tick；切换不 Cancel 旧能力。

### 4.10 连携技 Director（Phase 5）

目标进入 `State.Stun` → `AZZZCombatDirector`（GameState 子组件）调度：暂停敌人 AI（SlowMotion）+ 非活跃队友 IgnoreInput/Invulnerable → 连携选择 UI → `TryActivateAbilitiesByTag(Ability.ChainAttack)` → `AbilityTask_ChainCamera` 镜头 Lerp → 最多 3 次，已行动角色不可重复。

### 4.11 元素异常模型（Phase 4）

ExecCalc 统一计算 AnomalyBuildup → 目标施加对应 GE（Infinite+Stack）→ 达阈值触发异常爆发 GE（灼烧/冻结/感电/强击/侵蚀）→ FIFO 单异常 → 清空积蓄。属性变化走 Delegate（非 PostGameplayEffectExecute）。

### 4.12 敌人攻击预警黄闪（✅ 2026-08-08）

- 链路：蒙太奇抬手帧 → 引擎内置 "GameplayCue (Burst)" Notify（零 C++）→ `GameplayCue.ZZZ.EnemyAttackWarning` → `UGC_ZZZ_EnemyAttackWarning`（Static，Niagara 自灭，挂 `hand_r` socket，Emitter 勾 Local Space 跟随拳头；消除检查 State.Dead）。
- 注册表镜像坑（R20）：GCN 必须重写 `PostInitProperties`/`PostLoad`/`Serialize` 同步 `GameplayCueName = GameplayCueTag.GetTagName()` + `IsOverride=true` + ini 注册（`+Prop=` 语法），否则 CueManager 扫描 unmapped 静默丢弃；资产重存需先标 dirty。
- 扩展：红闪（不可闪避）/蓝闪（弹刀）同一 GCN 换色或按敌人子类配不同 cue tag。

## 五、实施路线图

| Phase | 内容 | 状态 |
|---|---|---|
| 1 | GAS 基础设施（AttributeSet/基类/ExecCalc/Tags/角色） | ✅ |
| 1.5 | 输入→Tag 桥接（UZZZInputConfig） | ✅ |
| 2 | 普攻连段 + 双窗口缓冲 + 伤害管线 + 飘字 | ✅（遗留：AbilityTask_DoTrace 推迟 Phase 5+） |
| 3 | 闪避/完美闪避/弹刀/突击/编队切换 | 🔄 进行中（见 Phase3 计划） |
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
| 特攻 (EX) | `UZZZSpecialAttack` → `GA_SpecialAttack` |
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
