# Phase 3 实施计划 — 闪避 / 招架(弹刀) / 突击 / 编队切换 + 3.5 时间管理 + 特殊技/能量

> **项目**: ZZZCombatRemake (UE 5.8) · 批准 2026-08-02 双审核通过
> **状态**（2026-09-11 对齐）：Phase 3 主体 ✅（Task 0–5a / 招架支援全链路 / Motion Warping / 屏幕 HUD + 敌人头顶条 / 招架早按必失败修复——细则见各节与 git log）。**2026-09-11 招架特写镜头 ✅ + rig 切换 tag 化 ✅（PIE 验证）**：C++ = `UZZZTagCameraDirector`（`UCameraDirector` 子类：每帧查角色 ASC owned tags → `TagMappings` 最高优先级 rig，无命中 → DefaultRig）+ `Camera.*` tag 层（`Camera.Closeup.Parry`）；资产 = `CA_ZZZCamera`（**新建 CA 时选 C++ director**）+ `CR_Closeup_Parry`（SetLocation/SetRotation·Pawn 锁机位 + FOV38）+ GA 的 `ActivationOwnedTags` 加 tag（**技能侧零代码**）。机制 + "换 director 的两条通道取舍"教训 = `ZZZ-Camera-Architecture.md` §四/§六.11–16。**大招分镜镜头基建 ⬜ 已写未验证**（`PlayCinematic`/`StopCinematic` + `ZZZ.PlayCinematic` 命令；归还 = CameraCut section 的 When Finished=Restore State）。——**下一步 = 连携技**（特写模板已就绪可直接复用：GA 加 tag + TagMappings 加一行）**或大招分镜验证**。
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
- **2026-09-03 完美闪避慢动作发放点定稿（C++ + 敌人蒙太奇 notify 已摆）**：判定仍按下时（黄闪窗）→ `UZZZDodge` 只给敌人挂 `Effect.Enemy.Dodged`（A 层 LooseTag，EndAbility 兜底）→ 敌人蒙太奇 notify 消费 → 自施 `UZZZGameplayEffect_SlowMotion`（1.0s/0.15，打空后起）。玩家侧慢放 0.5 不动。**资产已做**：AM_EnemyAttack 已摆 `ZZZEnemyDodgeSlow`（0.396s）与 `ZZZEnemyParryImpact`（0.392s）、黄闪 GC（0.067s）、AttackTrace 伤害帧（0.406s）、AbilityWindow（0.059–0.375s）。（以上均为**时间轴值**——真实时刻 = ÷RateScale，见「风险与坑」17）
- **2026-09-03 特殊技 + 能量系统（C++ ✅ + 珂蕾妲资产 ✅；输入缓冲并入同 commit）**：`UZZZSpecialAttack` 档位模型（Lead A/B/C ×3 蒙太奇 + Body 普通/强化 ×2 = GA_Koleda_SpecialAttack 已配：Direct=BA02/BA04、ComboLead=BA01/BA03/Dash.Counter、DashLead=Dash.Attack、EnergyCost 50、Asset Tags={Basic, Special}、NextCombo=DashAttack）；输入 = E（IA_ZZZSpecial 已建并绑定 IMC）；忙且无窗时进输入缓冲（开窗按 buffered tag 分流，Y 优先于连段）。能量：命中 +2、自然 1/s、隐藏队员照回。
- **2026-09-04 招架（AssistDefensive）C++ 全套**（当日未提交——随 09-05 落地同批提交；资产已做）：见「三、进行中」清单 + 架构文档 §4.9.2。
- **2026-09-05 敌人失衡恢复（PIE 验证通过）**：`UZZZGameplayEffect_Stun` 由 Infinite 改 **Duration 5.0s 自过期**；`ZZZAttributeSet` 失衡判据由 `bIsStunned` bool 改查 `State.Stun` tag 在场（bool 感知不到 GE 过期、会堵死二次失衡，已删）。原症状：Daze 满 → 永久眩晕 → 敌人永不恢复行动（普攻命中打断后"卡死"表象实为此，命中取消本身每次正常恢复——PIE 连续打断几十次均恢复）。
- **2026-09-05 调试事实**：PIE `showdebug AbilitySystem` 只显示**玩家自身** ASC（不随准星切目标）——核对阵营 tag（State.Player/Enemy）勿用它看敌人；敌人实测恒为 State.Alive/State.Enemy（失衡时 + State.Stun），代码无任何授敌 State.Player 的路径。
- **2026-09-07 招架/突击 Motion Warping（PIE 验证通过）**：引擎 `Animation/MotionWarping` 插件启用（.uproject + Build.cs）+ `AZZZCharacter` 挂 `UMotionWarpingComponent`（自动建 CharacterAdapter）。**敌人引用生命周期**：PC `TryParrySwitch` 摆位后 `SetParryEnemy` → 招架 GA 激活写落点 warp target `ZZZ_ParryLand`（= 敌 `hand_r` 骨骼 + 敌Forward×`ParryWarpForwardOffset`(40)，yaw 面敌）→ 突击 GA（FollowUpAttack 族）激活时**取走并清空** + 写 `ZZZ_RushLand`（= 敌正后方 `RushPassDistance`(200)，旋转 = 位移方向）→ 招架 GA EndAbility 兜底清。穿敌碰撞 = 突击蒙太奇 `CollisionPassThrough` 盖全程（GA_AssistRush 关 `bRotateToTarget`）。warp 窗口 = 蒙太奇 `AnimNotifyState_MotionWarping`（target 名须与 C++ 槽一致）；⚠ `MaxSpeedClampRatio` 保持 0。算法事实：SkewWarp 每帧按「剩余到目标距离 ÷ 剩余 root motion」重归一化 → 窗口末必落 target，动画位移长短不影响到达；观感速度 ∝ 目标距离/窗口时长。

## 三、进行中

### 招架支援 ✅（2026-09-05 定稿落地；定稿细节见架构文档 §4.9.2）

最终实现（取代早前"直调 OnParryImpact"草案——spec.Ability 是 CDO，直调会 ensure，2026-09-05 改事件路由）：切换键按下 → `TryParrySwitch`（PC）：`ParryDetectRadius`(300) 内最近 AttackWindow 敌人命中 → 候选成员摆**敌正前方** `AssistEntryDistance`(120，沿 E facing 推出、yaw 面向 E) → `BeginSwitchIn(false)` → Possess → 类扫描激活 `UZZZAssistDefensive`（Asset Tags=Ability.Defense.Assist）→ **激活成功才**挂 `Effect.Enemy.ParryPending`。敌人蒙太奇 0.392s `ZZZAnimNotify_EnemyParryImpact`：消费 → 敌自施冻结（槽 1，GE_ParryFreeze **0.3s**/0.01）→ `GameplayCue.ZZZ.ParryImpact` → **广播 `Event.Combat.ParryImpact`** → B 招架 GA 实例消费：`Montage_JumpToSection(Recover)` + 自施冻结（同帧，定格姿势=Recover 首帧）→ `CancelAbilities(Ability.Attack.Enemy)` → 敌人 Stagger（槽 2）。角色侧：招架期禁普攻起手 + 吞键（喂双任务）；**Recover 内先 `InputWindow(CanBuffer)` 死区再 `AbilityWindow(CanCombo)`** → 冻结期预按、开窗自动接支援突击（`NextComboAbility`=GA_AssistRush，走通用连招）；基类 `TrySetupComboHandoff()` 双窗口默认件 + 窗 tag 兜底清理集中基类 EndAbility。敌人攻击 GA EndAbility 兜底清 ParryPending。

**资产（✅ 2026-09-05 完成；09-07 warp 重建后名称/段界已按编辑器核对）**：`GA_Koleda_AssistDefence`（Asset Tags=Ability.Defense.Assist、Activation Owned Tags=State.Invulnerable、AttackMontage=`AM_Koleda_SwitchIn_Attack_Ex_Start_Freeze`、RecoverSectionName=Recover、NextComboAbility=GA_AssistRush、FreezeEffect=GE_ParryFreeze）；`GA_Koleda_AssistRush`（`UZZZFollowUpAttack` 子类 + `AM_Koleda_SwitchIn_Attack_Ex`）；招架蒙太奇 = `AM_Koleda_SwitchIn_Attack_Ex_Start_Freeze`（两段式 [Default 0→0.152][Recover 0.152→]，BlendIn=0，Recover 内两窗；旧 `…_Ex_Start` 已删，另存 `…_Ex_New` 用途待确认）；DefaultAbilities 已追加（Okuma 全、**Jane 未配招架 GA 会 warning**）。

**实战坑（2026-09-05 记录，勿再踩）**：① 源动画实际时长 < 蒙太奇段长 → 蒙太奇在段长前提前 blend-out（本次全链路卡死数日的病根——先用冲刺攻击隔离验证）；② 定格 1s 全停节奏拖死输入（0.3s 起调）；③ 招架蒙太奇 BlendIn 必须 0（idle→姿势混合稀释定格观感）；④ 段跳转 + 冻结爬行下 CanBuffer 起点需贴近段跳转点，窗才覆盖整个定格期。

### 剩余核对清单（2026-09-05 资产扫描确认仍缺）

- **GA_DashAttack/GA_DashCounter 身份 tag**：`Ability.Attack.Dash.Attack` / `Ability.Attack.Dash.Counter` 未补（GA_DashAttack AbilityTags 现为空——Special 的 DashLead/ComboLead 规则表引用它们，缺失则 dash→特殊技派生静默不触发）；GA_DashAttack `ActivationBlockedTags=State.PerfectDodge` 残留待清（纯手递手目标后已无意义）
- **Jane 战斗资产**：5 段普攻蒙太奇 + GA 01–05 已导入，但 DefaultAbilities 仅配 01–04；闪避/追击/特殊技/招架 GA + 蒙太奇未配（切换/退场蒙太奇已有）
- **AM_Dodge 窗 notify tag 核对**（Koleda 已按 CanCombo 跑通；其余角色补齐时同惯例：位移窗 CanCombo 须在 DodgeEnd notify 前结束）

### 未做

- **突击（连携突击）C++**：切换自动判定只实现了 AttackWindow→招架分支；Staggered(600)→突击分支（`UZZZAssistOffensive` 或同路径）未写；`SwitchToCharacter(Direction)` 参数化未做
- ~~**TeamPanel**（Task #6）~~ ✅ 2026-09-08（含头像/数值/开局预加载，见 `ZZZ-UI-Design.md`）
- ~~**敌人头顶条**~~ ✅ 2026-09-10（`AZZZCombatEnemy` HeadStatus WidgetComponent + `WBP_EnemyHead` 名字/血条/失衡条；挂点 90cm、DrawSize 220×40）

## 四、后续排程（2026-09-08 定序，替代旧 Task 依赖序）

**决策**：元素/异常（原 Phase 4）不做——仅玩法相关、与角色表现无关。按依赖与验收依赖定序：

1. **HUD & 敌人头顶条**（吸收 Task #6 TeamPanel；屏幕 HUD 与头顶条是两个载体）：
   - **屏幕 HUD** ✅ 2026-09-08 完成（小队栏槽序旋转 + 头像 + 血/能量 + HP 数值 + 技能按钮灰度就绪态；`OnPossess → RefreshSquad` + 属性 delegate 不轮询；**开局预加载全部成员**保证每槽实时数据）。细节见 `ZZZ-UI-Design.md`。
   - **敌人头顶条** ✅ 2026-09-10 完成（每敌人 `HeadStatus` UWidgetComponent，Screen 空间自动面向相机；名字 + 血条 + 失衡条，`BindStatus` 绑 delegate 不轮询）。
2. **连携技**（替代旧 Task #3 突击方向——原 Staggered 自动判定废弃）：复用失衡（`State.Stun` tag 判据）/冻结 GE/编队切换/相机特写预留全链；前提定夺：失衡→连携节奏（现 `_Stun` 5s 偏长，按怪种调，原"Phase 4+ 预备"项）；镜头特写模板在此趟出。
3. **大招**：新地基 = Decibel 全队共享宿主（宿主位置定夺——PlayerState 与 GameState 两处文档口径不一致，开工统一）；终结镜头复用 2 的特写基建。
4. **特效全量铺底**：旧技能 + 2/3 新技能一次覆盖不返工；接入 = GameplayCue + Niagara（R20 镜像 + 三档 cue tag + 打击帧 per-instance 开关基建已就绪）；试点可在 2/3 开发期先行立模板。

前置顺手项（不阻塞本序，人工资产操作时一并清）：§三「剩余核对清单」（DashAttack/Counter 身份 tag、Jane 战斗资产、AM_Dodge 窗核对）；快速支援预留接口维持挂起（未排程）。

## 五、风险与坑（实施时逐条对照）

1. **Triggered 语义**：Dodge/Switch 必须 Started（`bTriggerOnStarted`），否则按住连闪/连切。
2. **LooseTag 残留**：AnimNotifyState 被打断可能不触发 NotifyEnd → 窗口 tag 由能力 EndAbility 兜底移除（EnemyAttack 兜底清 AttackWindow/Dodged/ParryPending 三件套）；State.Attacking/Invulnerable 一律能力管理。
3. **M1（Duration GE）**：TimeDilation 桥接必须走 ValueChangeDelegate，PostGameplayEffectExecute 不触发。
4. **Timer lambda 悬垂**：硬直/顿帧定时器捕获 TWeakObjectPtr，禁止裸指针（现均 GE 化，新代码勿回退 Timer）。
5. **CancelSwitchOut 顺序**：先清标志再 Montage_Stop，否则 Interrupted 回调误隐藏新角色。
6. **动画骨架兼容**：占位动画必须珂蕾妲骨架；Mannequin 的 MM_Dash 不能用于 Okuma。
7. **PlayMontageAndWait 双回调**（BlendOut+Completed 各 End 一次）：引擎守卫，勿"修复"。**但 blend-out 在最后一段剩余播放 ≤ `BlendOutTriggerTime`（默认 = `BlendOut` 时长）时就触发**——没摆 EndEventTag notify 的技能，GA 寿命 = 蒙太奇时长 − BlendOut；需活到外部事件到场者（招架等打击帧、两段式 Lead）必须覆写 `OnMontageBlendOut()` 拦截（2026-09-10 招架 / 2026-09-03 特殊技），见 System-Design §4.7 结束位纪律。
8. **Root Motion 与程序化位移互斥**：二选一；Root Motion 时收刀段不关（10.1/8.10 原则）。
9. **Hit Stop/慢放/定格共享 CustomTimeDilation**：恢复值现读属性（属性为唯一真源），防覆盖他档。
10. **招架打断时序**：定格 → 取消 → 硬直全在敌人攻击 GA 存活期内完成（notify 0.392s 早于 GA 自然结束）；敌人攻击 GA 必须有 Asset Tags=Ability.Attack.Enemy 才能被 CancelAbilities 定位。蒙太奇 bStopWhenAbilityEnds=false → 取消后无主收尾，定格 GE 到期自然恢复。
11. **失衡判据必须走 tag**：Stun 自过期后以 `State.Stun` 在场判失衡（bool 感知不到 GE 过期——09-05 已踩）。
12. **多敌边界**：切换判定按状态优先级取最近敌人；FindNearestEnemy 一律过滤 hidden/State.Dead。
13. **弹刀取舍预期**：无精防窗口 + 无资源消耗 + 可零成本反复触发 = 原型取舍，验收按"无条件格挡换人"，不按 ZZZ 精防预期。
14. **SwitchOut 被打断竞态**：旧角色 SwitchOut 期间挂 State.Invulnerable（免伤拦截在扣血前短路）；Interrupted 分支仍隐藏兜底。
15. **5.8 API 事实**：`SetCustomTimeDilation` 已移除（直接属性赋值）；UHT 禁止 UFUNCTION 参数为 struct 裸指针（`const FGameplayEventData*`），用 GenericGameplayEventCallbacks + lambda。
16. **调试工具边界**：`showdebug AbilitySystem` 只显示玩家 ASC（不随准星切目标）；看敌人 tag 用实时日志/蓝图打印或关掉玩家遮挡后目测。
17. **敌人蒙太奇时间戳 = 时间轴值**：notify / 窗口 / 段界报的都是 timeline 秒，**真实时刻 = ÷RateScale**（`FAnimMontageInstance::Advance` 按 PlayRate×RateScale 推进位置）。AM_EnemyAttack 测试期设 0.6 → 打击帧真实 0.654s（不是 0.392s）、窗口 528ms；切回 1.0 才是文档里那组数。任何"按键窗口 vs notify 余量"的推算先换算——2026-09-10 招架 1 帧缺口就是这么算漏的。

## 六、关键文件

- `ZZZ/Abilities/ZZZGameplayAbility`（蒙太奇样板 + 组合交接基座）、`ZZZBasicAttack`、`ZZZEnemyAttack`、`ZZZDodge`、`ZZZFollowUpAttack`、`ZZZSpecialAttack`、**`ZZZAssistDefensive`（新，2026-09-04）**
- `ZZZ/Attributes/ZZZAttributeSet`（TimeDilation + 伤害/失衡/硬直触发点 + 失衡判据）
- `ZZZ/Player/ZZZPlayerController`（切换 + 招架自动判定 `TryParrySwitch`）
- `ZZZ/Enemies/ZZZCombatEnemy`（最小攻击行为 + TimeDilation 桥接 + FindNearestEnemy）
- `ZZZ/Animation/ZZZAnimNotify_EnemyParryImpact`、`ZZZAnimNotify_EnemyDodgeSlow`（新，敌人蒙太奇消费 notify）
- `ZZZ/Effects/ZZZStatusGameplayEffects`（_Stagger/_Stun(5s)/_Dead/_Alive/_ParryStop 等载体）
- `ZZZ/ZZZCharacter`（输入门控 + SwitchOut 接驳 + 招架期禁普攻）
- 新建（待做）：`ZZZ/Abilities/ZZZAssistOffensive`；`ZZZ/UI/ZZZPlayerHUDWidget`、`ZZZTeamPanelWidget`、`ZZZTeamPanelEntryWidget`、`ZZZSkillButtonWidget`（✅ 2026-09-08 已建，HUD 全套）
