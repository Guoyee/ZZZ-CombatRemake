# Phase 3 实施计划 — 闪避 / 招架(弹刀) / 突击 / 编队切换 + 3.5 时间管理 + 特殊技/能量

> **项目**: ZZZCombatRemake (UE 5.8) · 批准 2026-08-02 双审核通过
> **状态**（2026-09-05 全面核对同步）：Task 0–4 + Task 5a ✅；冲刺攻击/闪避反击 ✅（珂蕾妲资产完成、流程跑通，2026-09-02）；普通切换 ✅（2026-09-02）；慢动作发放点定稿 ✅ + 敌人蒙太奇 notify 已摆（2026-09-03，含招架 notify）；特殊技/能量 C++ + 珂蕾妲资产 ✅（2026-09-03，E 键已绑、输入缓冲已并入）；**招架（AssistDefensive）C++ 全套 ✅（2026-09-04，未提交）——资产（GA_AssistDefensive/GA_AssistRush + 蒙太奇）待做**；敌人失衡恢复 ✅（2026-09-05：`State.Stun` 改 Duration 5s 自过期）；TeamPanel ⬜；突击（Staggered 自动判定分支）C++ 未做
> **旧版备份**: `Docs/archive/ZZZ-Combat-Phase3-Plan.md.orig`（勿读，仅查证历史）

## 一、已确认决策

1. 闪避/弹刀/入场动画由用户导入（未导入前珂蕾妲动画占位；**Mannequin MM_Dash 骨架不兼容 Okuma，勿用**）。
2. 测试敌人用 Mannequin（SKM_Manny_Simple）。
3. 弹刀/突击 = 切换自动判定（原版即切换键，不区分按键）。
4. Phase 3.5 并入本次执行。
5. 弹刀窗口与极限闪避共用 `Effect.Enemy.AttackWindow`（黄闪同步段，2026-08-29）。
6. 窗口统一（2026-09-03 定稿）：CanDashAttack 废弃 → 通用 `CanCombo`（追击窗/连段窗/尾窗同一 notify 惯例）；追击技（冲刺攻击/闪避反击）= 闪避的连段段。
7. 失衡状态 = GE 管理（C 层）：`UZZZGameplayEffect_Stagger`（0.35s 自过期）+ `_Stun`（**2026-09-05 起 Duration 5s 自过期**，曾为 Infinite 致 Daze 满后永久卡死，PIE 复现确认修复）。

## 二、已完成（PIE 验证通过）

- **Task 0**：Tag 注册（Input.*、Ability.Defense.*、Ability.Attack.Enemy、State.*、Effect.Ability.*、Effect.Enemy.*、Event.Combat.*、GameplayCue.ZZZ.* 含 ParryImpact）；`TimeDilation` 桥接属性；阵营/存亡标记 GE（`UZZZGameplayEffect_Faction`/`_Alive`，应用时 DynamicGrantedTags）。
- **Task 1**：基类蒙太奇样板（PlayAttackMontage/PlayMontage + 4 回调 + EndEventTag）；`UZZZEnemyAttack`（Asset Tags=Ability.Attack.Enemy + ActivationOwnedTags=State.Attacking；EndAbility 兜底清 AttackWindow/Dodged/ParryPending）；敌人 Tick 攻击行为（AttackRange 120 / AggroRange 1000 / Cooldown 2.2；敌人攻击不携带 Daze）。
- **Task 2**：GA_Dodge（双蒙太奇、无敌=ActivationOwnedTags、二连闪 CD 0.7/0.7、程序化位移兜底）；InputConfig `bTriggerOnStarted`；`IsAbilityActiveWithTag` 泛化。
- **Task 3/3.5 收尾 + Task 4**：完美闪避判定前移（按下时黄闪窗查询）、双减速、震屏 `GC_ZZZ_CameraShake`（R20 三处重写）、TimeDilation 桥接（敌人+玩家）、HitStop、敌人受击硬直（Staggered）、FindNearestEnemy 助手（含 RequiredState，09-04 供招架共用）。
- **Task 5a（2026-08-16 重做为 notify 驱动）**：打击帧打击感（per-instance `bApplyHitStop`/`bApplyCameraShake` + 三档 cue tag；卡肉 = Duration GE 0.03s/0.01；pre-hit 快照防双反馈；`UZZZFollowUpAttack` 反击移除玩家慢放）；`GC_ZZZ_DamageNumber` 补 R20。
- **2026-09-02 冲刺攻击/闪避反击（珂蕾妲资产完成、流程跑通）**：GA_DashAttack（蒙太奇 AM_Attack_Rush，NextCombo=GA_BasicAttack_02）/ GA_DashCounter（AM_Attack_Counter）；GA_Dodge 追击双槽（DashFollowUp/PerfectFollowUp）已配。
- **2026-09-02 普通切换**：新人物立即进场（`BeginSwitchIn`，右后方）+ 旧人物异步退场状态机（等攻击 GA → `ExitMontage`/`RunningExitMontage` → 材质淡出 → 隐藏广播）；`bIsSwitching` + 竞态双守卫；相机走 GameplayCameras manager 过渡（另见相机文档）。
- **2026-09-03 攻击族基础件**：基类组合交接 `TrySetupComboHandoff`/`GetComboNext`；`EndEventTag` 默认 `Event.Combat.AttackEnd`（ini 预注册）；穿敌 notify `CollisionPassThrough` + `RotationOverride`；`Move()` 收刀 tag 显式清理。
- **2026-09-03 完美闪避慢动作发放点定稿（C++ + 敌人蒙太奇 notify 已摆）**：判定仍按下时（黄闪窗）→ `UZZZDodge` 只给敌人挂 `Effect.Enemy.Dodged`（A 层 LooseTag，EndAbility 兜底）→ 敌人蒙太奇 notify 消费 → 自施 `UZZZGameplayEffect_SlowMotion`（1.0s/0.15，打空后起）。玩家侧慢放 0.5 不动。**资产已做**：AM_EnemyAttack 已摆 `ZZZEnemyDodgeSlow`（0.396s）与 `ZZZEnemyParryImpact`（0.392s）、黄闪 GC（0.067s）、AttackTrace 伤害帧（0.406s）、AbilityWindow（0.059–0.375s）。
- **2026-09-03 特殊技 + 能量系统（C++ ✅ + 珂蕾妲资产 ✅；输入缓冲并入同 commit）**：`UZZZSpecialAttack` 档位模型（Lead A/B/C ×3 蒙太奇 + Body 普通/强化 ×2 = GA_Koleda_SpecialAttack 已配：Direct=BA02/BA04、ComboLead=BA01/BA03/Dash.Counter、DashLead=Dash.Attack、EnergyCost 50、Asset Tags={Basic, Special}、NextCombo=DashAttack）；输入 = E（IA_ZZZSpecial 已建并绑定 IMC）；忙且无窗时进输入缓冲（开窗按 buffered tag 分流，Y 优先于连段）。能量：命中 +2、自然 1/s、隐藏队员照回。
- **2026-09-04 招架（AssistDefensive）C++ 全套（未提交；资产待做）**：见「三、进行中」清单 + 架构文档 §4.9.1。
- **2026-09-05 敌人失衡恢复（PIE 验证通过）**：`UZZZGameplayEffect_Stun` 由 Infinite 改 **Duration 5.0s 自过期**；`ZZZAttributeSet` 失衡判据由 `bIsStunned` bool 改查 `State.Stun` tag 在场（bool 感知不到 GE 过期、会堵死二次失衡，已删）。原症状：Daze 满 → 永久眩晕 → 敌人永不恢复行动（普攻命中打断后"卡死"表象实为此，命中取消本身每次正常恢复——PIE 连续打断几十次均恢复）。
- **2026-09-05 调试事实**：PIE `showdebug AbilitySystem` 只显示**玩家自身** ASC（不随准星切目标）——核对阵营 tag（State.Player/Enemy）勿用它看敌人；敌人实测恒为 State.Alive/State.Enemy（失衡时 + State.Stun），代码无任何授敌 State.Player 的路径。

## 三、进行中

### 招架支援（弹刀）— C++ 就绪，资产待做

代码（2026-09-04，已编译）：切换键按下 → `TryParrySwitch`（PC）：`ParryDetectRadius`(300) 内最近 AttackWindow 敌人命中 → 候选成员摆敌正前方 `AssistEntryDistance`(120) → `BeginSwitchIn(false)`（不播 EnterMontage，入场演出=招架蒙太奇）→ Possess → 类扫描激活 `UZZZAssistDefensive`（Asset Tags 须含 `Ability.Defense.Assist`）→ **激活成功才**给敌人挂 `Effect.Enemy.ParryPending`（失败 = 敌人攻击照常，不产生定格）。敌人蒙太奇 0.392s `ZZZAnimNotify_EnemyParryImpact`：消费 ParryPending → 敌自施 `UZZZGameplayEffect_ParryStop`（0.2s/0.01 定格）→ `GameplayCue.ZZZ.ParryImpact` → 扫玩家活动招架 GA 直调 `OnParryImpact`（同帧定格玩家，bParryImpacted 防重）→ `CancelAbilities(Ability.Attack.Enemy)`（GA End → 蒙太奇无主播完收尾）→ 敌人 Stagger。角色侧：招架 GA 活动期间禁普攻起手；收势尾窗 CanCombo → `AssistFollowUpAbility`（支援突击）手递手。敌人攻击 GA EndAbility 兜底清 ParryPending。

**资产待做（人工，照 GA_Koleda_SpecialAttack 装配惯例）**：
1. `GA_AssistDefensive`（基 `UZZZAssistDefensive`：Asset Tags=Ability.Defense.Assist；AttackMontage=招架蒙太奇；追击槽 `AssistFollowUpAbility`）+ 招架蒙太奇（**起手即招架姿势**，收势尾窗挂 CanCombo，姿势段覆盖敌人 0.392s 定格帧——入口 120cm + 敌人 1.2 倍缩放实测贴合）
2. `GA_AssistRush`（支援突击，同追击技惯例：无触发器、Asset Tags 补 `Ability.Attack.Dash.Assist` 类身份 tag）
3. 每角色 DefaultAbilities 追加 + PIE（黄闪窗内切换 → 招架定格 → 敌人硬直后恢复（需 09-05 失衡修复在场）→ 尾窗按攻击出支援突击）

### 剩余核对清单（2026-09-05 资产扫描确认仍缺）

- **GA_DashAttack/GA_DashCounter 身份 tag**：`Ability.Attack.Dash.Attack` / `Ability.Attack.Dash.Counter` 未补（GA_DashAttack AbilityTags 现为空——Special 的 DashLead/ComboLead 规则表引用它们，缺失则 dash→特殊技派生静默不触发）；GA_DashAttack `ActivationBlockedTags=State.PerfectDodge` 残留待清（纯手递手目标后已无意义）
- **Jane 战斗资产**：5 段普攻蒙太奇 + GA 01–05 已导入，但 DefaultAbilities 仅配 01–04；闪避/追击/特殊技/招架 GA + 蒙太奇未配（切换/退场蒙太奇已有）
- **AM_Dodge 窗 notify tag 核对**（Koleda 已按 CanCombo 跑通；其余角色补齐时同惯例：位移窗 CanCombo 须在 DodgeEnd notify 前结束）

### 未做

- **突击（连携突击）C++**：切换自动判定只实现了 AttackWindow→招架分支；Staggered(600)→突击分支（`UZZZAssistOffensive` 或同路径）未写；`SwitchToCharacter(Direction)` 参数化未做
- **TeamPanel**（Task #6）

## 四、后续 Task（依赖顺序）

- **Task #3 剩余**：突击分支 C++（见三）。
- **Task #5（资产+验证）**：三、中「资产待做 + 核对清单」全部；验证：弹刀 → 敌人硬直 → 失衡/恢复 → 再切换变突击（机制串联）；压力测试（三人快速切换 10+ 次无崩溃/tag 残留/CameraShake 堆叠）。
- **Task #6**：TeamPanel —— override `OnPossess` → RefreshSquad（替代延迟一帧）；Entry 绑属性变化 delegate（不轮询）+ State.Dead 灰显。
- **Phase 4+ 预备**：连携窗口细化时按怪种调 `_Stun` 时长（现 5s 默认，BP 子类可覆写）；失衡期间 Daze 积累可后续按设计收紧。

## 五、风险与坑（实施时逐条对照）

1. **Triggered 语义**：Dodge/Switch 必须 Started（`bTriggerOnStarted`），否则按住连闪/连切。
2. **LooseTag 残留**：AnimNotifyState 被打断可能不触发 NotifyEnd → 窗口 tag 由能力 EndAbility 兜底移除（EnemyAttack 兜底清 AttackWindow/Dodged/ParryPending 三件套）；State.Attacking/Invulnerable 一律能力管理。
3. **M1（Duration GE）**：TimeDilation 桥接必须走 ValueChangeDelegate，PostGameplayEffectExecute 不触发。
4. **Timer lambda 悬垂**：硬直/顿帧定时器捕获 TWeakObjectPtr，禁止裸指针（现均 GE 化，新代码勿回退 Timer）。
5. **CancelSwitchOut 顺序**：先清标志再 Montage_Stop，否则 Interrupted 回调误隐藏新角色。
6. **动画骨架兼容**：占位动画必须珂蕾妲骨架；Mannequin 的 MM_Dash 不能用于 Okuma。
7. **PlayMontageAndWait 双回调**（BlendOut+Completed 各 End 一次）：引擎守卫，勿"修复"。
8. **Root Motion 与程序化位移互斥**：二选一；Root Motion 时收刀段不关（10.1/8.10 原则）。
9. **Hit Stop/慢放/定格共享 CustomTimeDilation**：恢复值现读属性（属性为唯一真源），防覆盖他档。
10. **招架打断时序**：定格 → 取消 → 硬直全在敌人攻击 GA 存活期内完成（notify 0.392s 早于 GA 自然结束）；敌人攻击 GA 必须有 Asset Tags=Ability.Attack.Enemy 才能被 CancelAbilities 定位。蒙太奇 bStopWhenAbilityEnds=false → 取消后无主收尾，定格 GE 到期自然恢复。
11. **失衡判据必须走 tag**：Stun 自过期后以 `State.Stun` 在场判失衡（bool 感知不到 GE 过期——09-05 已踩）。
12. **多敌边界**：切换判定按状态优先级取最近敌人；FindNearestEnemy 一律过滤 hidden/State.Dead。
13. **弹刀取舍预期**：无精防窗口 + 无资源消耗 + 可零成本反复触发 = 原型取舍，验收按"无条件格挡换人"，不按 ZZZ 精防预期。
14. **SwitchOut 被打断竞态**：旧角色 SwitchOut 期间挂 State.Invulnerable（免伤拦截在扣血前短路）；Interrupted 分支仍隐藏兜底。
15. **5.8 API 事实**：`SetCustomTimeDilation` 已移除（直接属性赋值）；UHT 禁止 UFUNCTION 参数为 struct 裸指针（`const FGameplayEventData*`），用 GenericGameplayEventCallbacks + lambda。
16. **调试工具边界**：`showdebug AbilitySystem` 只显示玩家 ASC（不随准星切目标）；看敌人 tag 用实时日志/蓝图打印或关掉玩家遮挡后目测。

## 六、关键文件

- `ZZZ/Abilities/ZZZGameplayAbility`（蒙太奇样板 + 组合交接基座）、`ZZZBasicAttack`、`ZZZEnemyAttack`、`ZZZDodge`、`ZZZFollowUpAttack`、`ZZZSpecialAttack`、**`ZZZAssistDefensive`（新，2026-09-04）**
- `ZZZ/Attributes/ZZZAttributeSet`（TimeDilation + 伤害/失衡/硬直触发点 + 失衡判据）
- `ZZZ/Player/ZZZPlayerController`（切换 + 招架自动判定 `TryParrySwitch`）
- `ZZZ/Enemies/ZZZCombatEnemy`（最小攻击行为 + TimeDilation 桥接 + FindNearestEnemy）
- `ZZZ/Animation/ZZZAnimNotify_EnemyParryImpact`、`ZZZAnimNotify_EnemyDodgeSlow`（新，敌人蒙太奇消费 notify）
- `ZZZ/Effects/ZZZStatusGameplayEffects`（_Stagger/_Stun(5s)/_Dead/_Alive/_ParryStop 等载体）
- `ZZZ/ZZZCharacter`（输入门控 + SwitchOut 接驳 + 招架期禁普攻）
- 新建（待做）：`ZZZ/Abilities/ZZZAssistOffensive`；`ZZZ/UI/ZZZTeamPanelWidget`、`ZZZTeamPanelEntryWidget`
