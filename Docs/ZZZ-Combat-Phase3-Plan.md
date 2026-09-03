# Phase 3 实施计划 — 闪避 / 弹刀 / 突击 / 编队切换 + 3.5 时间管理

> **项目**: ZZZCombatRemake (UE 5.8) · 批准 2026-08-02 双审核通过
> **状态**: 实施中 —— Task 3/4 C++ 完成；**冲刺攻击/闪避反击 ✅（珂蕾妲资产完成、流程跑通，2026-09-02）**；**A 层窗口 GE 化已取消**（2026-08-29 定稿：窗口 tag 保持 LooseTag，C/B 层状态 GE 已落地）；**普通切换已落地（2026-09-02，见架构文档 §4.9.1）**；**特殊技（普通/强化 + 快速派生）+ 能量系统 C++ ✅ 2026-09-03**（详见架构文档 §4.13；资产待做）；Assist（弹刀/突击）C++、切换自动判定、TeamPanel 待做
> **旧版备份**: `Docs/archive/ZZZ-Combat-Phase3-Plan.md.orig`（勿读，仅查证历史）

## 一、已确认决策

1. 闪避/弹刀/入场动画由用户导入（未导入前珂蕾妲动画占位；**Mannequin MM_Dash 骨架不兼容 Okuma，勿用**）。
2. 测试敌人用 Mannequin（SKM_Manny_Simple）。
3. 弹刀/突击 = 切换自动判定（原版即切换键，不区分按键）。
4. Phase 3.5 并入本次执行。

## 二、已完成（PIE 验证通过）

- **Task 0**：Tag 注册（Input.Dodge/Switch.Prev、Ability.Defense.*、Ability.Attack.Enemy、State.Invulnerable/SlowMotion/Enemy/Player/Attacking、Effect.Ability.CanDodge/CanParry、GameplayCue.ZZZ.CameraShake）；`TimeDilation` 属性；阵营标记 GE（`UZZZGameplayEffect_Faction`，应用时 DynamicGrantedTags）。
- **Task 1**：基类蒙太奇样板上移（PlayAttackMontage/PlayMontage + 4 回调 + EndEventTag）；`UZZZEnemyAttack`（AbilityTags=Ability.Attack.Enemy + ActivationOwnedTags=State.Attacking）；敌人 Tick 攻击行为（AttackRange **120** / AggroRange 1000 / Cooldown 2.2；**敌人攻击不携带 Daze**）；GA_EnemyAttack + AM_EnemyAttack + BP_EnemyTest_Manny。
- **Task 2**：GA_Dodge（方向选蒙太奇、无敌=ActivationOwnedTags、二连闪 CD 0.7/0.7、程序化位移兜底 11000 cm/s²/0.22s）；InputConfig 加 `bTriggerOnStarted`；`IsAbilityActiveWithTag` 泛化。
- **Task 3/3.5 收尾 + Task 4**：完美闪避判定前移（按下时窗口查询）、双减速（敌人 0.15 立即 + 玩家 0.5 notify 驱动）、震屏 `GC_ZZZ_CameraShake`（含 R20 三处重写）、TimeDilation 桥接（敌人+玩家）、ApplyHitStop、敌人受击硬直（Staggered）、FindNearestEnemy 助手、`GC_ZZZ_DamageNumber` 补 R20。
- **Task 5a（2026-08-16 重做为 notify 驱动）**：打击帧打击感——`UZZZAnimNotify_AttackTrace` 新增 per-instance 开关（`bApplyHitStop` + 默认 GE `UZZZGameplayEffect_HitStop`、`bApplyCameraShake` + `CameraShakeCueTag` FGameplayTag 三档 Low/Mid/High 默认 Low，ini 注册；一个 tag 只挂一个处理器，共用单一 `BP_HitShake` 资产）；卡肉 = GE（Duration 0.03s 世界时间 + TimeDilation Override 0.01——LOW 档默认，重击换 BP 子类；到期 aggregator 恢复，无定时器无守卫）；命中保证 = 空挥不进循环 + pre-hit 快照（Invulnerable/Dead，击杀帧保留反馈）；反击激活移除玩家慢放（`UZZZFollowUpAttack` 的 `RemoveActiveEffectsWithGrantedTags(State.SlowMotion)`）。旧 ApplyHitStop 机制全删（两角色）。⚠ 桥接的组件抽取仍推迟至第三个 Actor 类出现。
- **新 Tag 已注册**：`Effect.Enemy.AttackWindow`、`State.PerfectDodge`、`Effect.Ability.CanDashAttack`、`Event.Combat.DodgeSlowStart`。
- **2026-09-02 攻击族基础件（构建通过，详见架构文档 §3.2/§4.2/§4.3/§4.7）**：基类组合交接 `TrySetupComboHandoff`（WaitCombo/连段过渡自 BasicAttack 上移，opt-in——BasicAttack 无条件调用保留终端 flush，FollowUpAttack 配 Next 才调用）；`EndEventTag` 未配置默认 `Event.Combat.AttackEnd`（ini 预注册防 CDO ensure）；穿敌 notify `AnimNotifyState_CollisionPassThrough`（`WindowTag=State.PassThrough`，RotateToTarget tick 门控停转向）+ 旋转覆盖 `AnimNotifyState_RotationOverride`；`Move()` 收刀 tag 消费方清理（2026-08-29 已修）。dash→普攻2 链出需资产侧补：GA_DashAttack 配 `NextComboAbility=GA_BasicAttack_02` + 冲刺蒙太奇收刀段挂 CanCombo 窗口。
- **2026-09-03 特殊技 + 能量系统（构建通过；定稿见架构文档 §4.13）**：`UZZZSpecialAttack` 档位模型——可选起手 A/B/C（Lead，含弱打击1，普通/强化共用）+ 主体 Body 普通/强化（仅打击2；能量≥EnergyCost 判定+扣费在 GA 内）；入口 = 激活时自扫前驱活动 GA 资产 tag 匹配 Direct/ComboLead/DashLead 规则表（`HasTagExact`，GA BP 配置；2/4 段免起手直连主体）；两段式播放（Lead OnCompleted 切 Body，`bLeadPending`）；Asset Tags={Basic, Special}；清慢放 GE；EndAbility 兜底清窗 tag。**窗口统一（定稿）**：CanDashAttack 废弃 → 通用 CanCombo（dodge 位移追击窗、普攻连段窗、攻击尾窗同一 notify 惯例）；**追击技重构为连段段**——无 AbilityTriggers，GA_Dodge 追击双槽（`DashFollowUpAbility`/`PerfectFollowUpAbility`）经 `TrySetupComboHandoff` + `GetComboNext()`（按下时完美判定）手递手；`NextComboAbility` 放宽为 UGameplayAbility 族；门控只做"闪避活动+CanCombo 窗 → 跳过普攻起手"。`Energy/MaxEnergy` + `UZZZGameplayEffect_EnergyDelta`（C++ SetByCaller 载体，Data.Energy ini 预注册——5.8 幅值结构修正 FSetByCallerFloat ctor）；命中回能（AttributeSet 伤害确认单点）+ 自然回能（世界 FTimer）；Y 门控 `TryActivateSpecialAttack` 简化（窗口={CanCombo, Recovery}+自由态；忙=类扫描；不传上下文）。**资产待做**（步骤见架构文档 §4.13 与 CLAUDE.md 当前状态；含窗口统一的人工改动：AM_Dodge 位移窗 notify → CanCombo、GA_DashAttack/Counter 触发 tag/Required 更新、每角色 5 条特殊技蒙太奇）。

关键实现定稿（后续工作依赖）：
- **两段式统一机制**：基类 `EndEventTag`（DodgeEnd / AttackEnd）+ 过渡段无主播放（bStopWhenAbilityEnds=false）。
- **弹刀窗口与极限闪避共用 `Effect.Enemy.AttackWindow`**（黄闪同步段）。
- **硬直统一 `UZZZGameplayEffect_Stagger`**（Duration 0.35s，替代 LooseTag+Timer）。

## 三、进行中：冲刺攻击/闪避反击（代码未写）

1. `Effects/ZZZStatusGameplayEffects`：`_Stagger`（0.35s）/ `_Dead` / `_Stun` / `_Alive`（Infinite）**已落地** ✅（C/B 层，tag 一律应用时 `DynamicGrantedTags`，照 Faction 模式）。`UZZZGameplayEffect_WindowTag`（Infinite）**已取消** ⛔（2026-08-29 定稿：A 层窗口保持 LooseTag）。
2. ~~`AnimNotifyState_AbilityWindow`/`InputWindow` GE 化~~ **已取消** ⛔（2026-08-29 定稿：notify 共享实例下 LooseTag 最简洁；A 层窗口 tag = notify 配对 LooseTag + 双轨兜底 —— 有主段 EndAbility、无主段消费方（如 `Move()`）显式清理）。
3. `UZZZDodge`：`State.PerfectDodge` → Duration GE（`GE_PerfectDodge_Status`：**0.5s 固定** + TargetTags=State.PerfectDodge，不用 SetByCaller）；删 AddLooseGameplayTag；EndAbility 兜底 RemoveLooseGameplayTag(**CanCombo**——2026-09-03 窗口统一，原 CanDashAttack)。
4. `ZZZAttributeSet`：Staggered/Dead/Stun → 对应 GE 施加（删 Timer lambda）。
5. `ZZZCombatEnemy`：State.Alive → `_Alive` GE。
6. `ZZZCharacter` 起手守卫：闪避中且 `CanDashAttack` → 跳过普攻起手（交给 DashAttack/DodgeCounter 的 AbilityTriggers 路由）。
7. 资产（用户操作）：
   - `GE_PerfectDodge_Status`；`GE_SlowMotion` Duration ≈ **1.0s**、`GE_PlayerSlowMotion` ≈ **与 CanDashAttack 窗口同步**（手感调；GE 时长走世界时间，角色膨胀不影响——2026-08-15 查证）；`GE_PlayerSlowMotion` 加 **TargetTags 组件 = `State.SlowMotion`**（反击移除定位用，规则 3 GEComponents）
   - `GA_DashAttack`（Trigger=Input.Attack + Required=CanDashAttack + **Blocked=PerfectDodge**）；`GA_DodgeCounter`（额外 Required=PerfectDodge）
   - `AM_Dodge_Fwd/Back`：位移段 AbilityWindow(**CanCombo**——2026-09-03 窗口统一，原 CanDashAttack 废弃；窗口须在 DodgeEnd notify 前结束，追击交接在闪避 GA 存活期内) + 位移末段 SendGameplayEvent(`Event.Combat.DodgeSlowStart`，DodgeEnd notify 之前)
   - `GA_DashAttack`/`GA_DodgeCounter`：**AbilityTriggers 清空**（2026-09-03 重构为连段段——纯手递手目标，无触发器；Required/Blocked 全部移除）；**Asset Tags 补段身份 tag**（2026-09-03 注册 `Ability.Attack.Dash.Attack` / `Ability.Attack.Dash.Counter`，保留原 Ability.Attack.Basic 等既有 tag）；GA_Dodge 新增追击双槽：`DashFollowUpAbility=GA_DashAttack`、`PerfectFollowUpAbility=GA_DodgeCounter`
   - 三角色 DefaultAbilities 追加 GA_DashAttack/GA_DodgeCounter
   - PIE：普通闪避位移段按攻击→冲刺；完美闪避位移段按攻击→反击（慢放中）；窗口外→普攻不变

## 四、后续 Task（依赖顺序）

- **Task #3**：`UZZZAssistDefensive` / `UZZZAssistOffensive` C++。弹刀已按 `Effect.Enemy.AttackWindow` 设计（含敌人 Staggered GE 化后改施加方式）；**取舍显式标注：无精防窗口 + 无资源消耗 → Phase 5**。
- **Task #4**：PC 切换重构 —— ✅ **普通切换部分已落地（2026-09-02）**：新人物立即进场（`BeginSwitchIn`，入场位置 = 旧人物右后方）+ 旧人物异步退场状态机（`StartSwitchOut`：等攻击 GA EndAbility → 退场动画按 `bSwitchWaitedForAbility` 选 `ExitMontage`/`RunningExitMontage` → 材质淡出并行、`FadeDuration` 控制隐藏 → `FinalizeSwitchOut` 广播）；`bIsSwitching` 守卫 + 竞态双守卫（基类 `OnComboHandoffTriggered`〔原 `CheckComboTransition`，2026-09-02 随组合交接上移基类〕/`WaitCombo::OnComboWindowChanged` 查 `IsSwitchingOut`）；相机走 `OnPossess` manager Push（见架构文档 §4.9.1 与 Camera 文档）。**剩余**：`SwitchToCharacter(Direction)` 参数化 + 自动判定（AttackWindow(300)→弹刀 / Staggered(600)→突击 / 普通）；激活失败回落普通切换表现；阵亡成员跳过（IsSquadClassEliminated 已覆盖）。
- **Task #5**：资产+验证 —— AM_SwitchOut（包装 SwitchOut_Normal_Anim1）、AM_AssistDefensive/Offensive（占位 AM_BasicAttack_01）、GA×2、IA_ZZZSwitchPrev(Q)+IMC+DA、BP_Okuma_Third、PC 配置；验证：弹刀→敌人硬直→再按切换变突击（机制串联）、压力测试（三人快速切换 10+ 次无崩溃/tag 残留/CameraShake 堆叠）。
- **Task #6**：TeamPanel —— override `OnPossess` → RefreshSquad（替代延迟一帧）；Entry 绑属性变化 delegate（不轮询）+ State.Dead 灰显。
- **Task #7**：`GC_ZZZ_DamageNumber` 重存（用户操作，R20）。

## 五、风险与坑（实施时逐条对照）

1. **Triggered 语义**：Dodge/Switch 必须 Started（`bTriggerOnStarted`），否则按住连闪/连切。
2. **LooseTag 残留**：AnimNotifyState 被打断可能不触发 NotifyEnd → 窗口 tag 由能力 EndAbility 兜底移除；State.Attacking/Invulnerable 一律能力管理。
3. **M1（Duration GE）**：TimeDilation 桥接必须走 ValueChangeDelegate，PostGameplayEffectExecute 不触发。
4. **Timer lambda 悬垂**：硬直/顿帧定时器捕获 TWeakObjectPtr，禁止裸指针。
5. **CancelSwitchOut 顺序**：先清标志再 Montage_Stop，否则 Interrupted 回调误隐藏新角色。
6. **动画骨架兼容**：占位动画必须珂蕾妲骨架；Mannequin 的 MM_Dash 不能用于 Okuma。
7. **PlayMontageAndWait 双回调**（BlendOut+Completed 各 End 一次）：引擎守卫，勿"修复"。
8. **Root Motion 与程序化位移互斥**：二选一；Root Motion 时收刀段不关（10.1/8.10 原则）。
9. **Hit Stop 与慢动作共享 CustomTimeDilation**：恢复值现读属性（属性为唯一真源），防覆盖 0.15 慢动作。
10. **弹刀打断时序**：AssistDefensive 先 Cancel 敌人攻击再播入场蒙太奇；敌人攻击 GA 必须有 AbilityTags=Ability.Attack.Enemy 才能被定位。
11. **多敌边界**：切换判定按状态优先级取最近敌人；FindNearest* 一律过滤 hidden/State.Dead。
12. **弹刀取舍预期**：无精防窗口 + 无资源消耗 + 可零成本反复触发 = 原型取舍，验收按"无条件格挡换人"，不按 ZZZ 精防预期。
13. **SwitchOut 被打断竞态**：旧角色 SwitchOut 期间挂 State.Invulnerable（免伤拦截在扣血前短路）；Interrupted 分支仍隐藏兜底。
14. **5.8 API 事实**：`SetCustomTimeDilation` 已移除（直接属性赋值）；UHT 禁止 UFUNCTION 参数为 struct 裸指针（`const FGameplayEventData*`），用 GenericGameplayEventCallbacks + lambda。

## 六、关键文件

- `ZZZ/Abilities/ZZZGameplayAbility`（蒙太奇样板基座）、`ZZZBasicAttack`、`ZZZEnemyAttack`、`ZZZDodge`
- `ZZZ/Attributes/ZZZAttributeSet`（TimeDilation + 完美闪避/免伤/硬直/HitStop 触发点）
- `ZZZ/Player/ZZZPlayerController`（切换自动判定/三人循环/TeamPanel 接线）
- `ZZZ/Enemies/ZZZCombatEnemy`（最小攻击行为 + TimeDilation 桥接 + Hit Stop）
- `ZZZ/ZZZCharacter`（输入门控 + SwitchOut 接驳）
- 新建：`ZZZ/Abilities/ZZZAssistDefensive`、`ZZZAssistOffensive`；`ZZZ/Effects/ZZZStatusGameplayEffects`；`ZZZ/UI/ZZZTeamPanelWidget`、`ZZZTeamPanelEntryWidget`
