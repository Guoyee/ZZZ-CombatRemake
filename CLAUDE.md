# CLAUDE.md

UE 5.8 单机 C++ 项目，复刻《绝区零》(Zenless Zone Zero) 核心战斗逻辑，GAS（Gameplay Ability System）为架构核心。

## 必读文档

- `Docs/ZZZ-Combat-System-Design.md` — 唯一权威架构设计，开发前必读
- `Docs/ZZZ-Combat-Phase3-Plan.md` — 当前实施进度与待办（Phase 3 进行中）
- `Docs/ZZZ-Camera-Architecture.md` — 相机架构定稿（GameplayCameras manager 模式 + 切人过渡 + 特写预留），开发前必读
- `Docs/archive/` — 旧版完整文档备份，**勿读**（仅人工查证历史时使用）

## 构建

命令（生成工程文件 / 构建编辑器目标 / 打开项目）见 `Docs/Setup.md`。
纪律：小改动可用 Live Coding；**每个实施 Task 完成即构建 + PIE 手测，不跨 Task 堆积**（新增 UCLASS / UPROPERTY 必须完整构建，UHT 重跑）。

## 架构速览

- 单模块 `Source/ZZZCombatRemake/`，依赖 EnhancedInput / AIModule / StateTreeModule / GameplayStateTreeModule / UMG / GameplayAbilities。
- `ZZZ/` 是目标模块（GAS 战斗）；`Variant_*` 是模板参考代码，**勿改**。
- 角色 = BP 子类 + 每角色自己的 GA BP 资产（`DefaultAbilities`）；不建 per-character C++ 类。输入配置（`UZZZInputConfig`，IA→GameplayTag）全局共享。
- 核心类：`AZZZCharacter`（Pawn-ASC）/ `AZZZCombatEnemy` / `UZZZAttributeSet` / `UZZZGameplayAbility` 及子类 / `UZZZDamageExecution`（统一 ExecCalc：HP+Daze+Anomaly）。
- 命名：`Elimination` 非 `Kill`；属性 `Health` 非 `CurrentHP`；GameplayCue 加 `ZZZ_` 前缀。
- UE 5.8 官方文档：https://docs.unrealengine.com/5.8/en-US/ （API 疑问优先查官方）

## 硬性规则（命令层——编号被 Phase3-Plan / System-Design / memory 引用，勿重排；定稿细节与坑以 System-Design 对应节为准，勿在本节复述细则）

1. **Tag 分层，按场景选机制**（A–E 五层全文见 System-Design §3.4 + §3.5）：
   - A 动画窗口（CanCombo/CanBuffer/Enemy.AttackWindow/State.Combat.Recovery/State.PassThrough 等）= notify 配对 LooseTag + **双轨兜底**（有主段：消费能力 EndAbility 移除；无主段：消费方如 `Move()` 显式 `RemoveLooseGameplayTag`）；禁止 notify 之外随手 Add。
   - B 持久身份 = Infinite GE + 应用时 DynamicGrantedTags；C 核心状态（Dead/Stun/Staggered）= **GE 管理，禁 LooseTag**；D 能力生命周期（Attacking/Invulnerable）= ActivationOwnedTags；E 本地路由标志 = Duration GE 自过期。
   - 废弃 tag（CanDodge/CanParry/CanDashAttack）：保留注册防旧资产，勿用。GE 载体：系统级零参数用 C++ 载体类，能力级可调用 BP 资产 + TSubclassOf。
2. **GE CDO 时序**：CDO 构造早于模块 StartupModule，构造函数内 `FZZZGameplayTags::Get()` 无效（配进 CDO 的 Tag 静默丢弃）→ Tag 一律**应用时**解析；构造函数内用 CreateDefaultSubobject，禁 NewObject（Fatal）。
3. **UE 5.3+ GE 组件**：`GrantedTags` / Application Tag Requirements 等直接属性已废弃 → 用 GEComponents（UTargetTagsGameplayEffectComponent / UTargetTagRequirementsGameplayEffectComponent）。
4. **GCN 注册表镜像（R20）**：GCN 必须重写 PostInitProperties/PostLoad/Serialize 同步 `GameplayCueName = GameplayCueTag.GetTagName()`；`IsOverride=true`；cue tag 需 ini 注册。完整见 System-Design §4.12。
5. **视觉反馈走 GameplayCue**：禁止在 `PostGameplayEffectExecute` 内 SpawnEmitter → `ExecuteGameplayCueOnActor`。
6. **M1 回调区分**：`PostGameplayEffectExecute` 仅对 Instant GE 触发；Duration/Infinite GE 属性变化走 `GetGameplayAttributeValueChangeDelegate()`。
7. **收刀 = 连段窗口 + 标准打断**（完整流程见 System-Design §4.7，新技能一律照此）：① 动作段末 notify（`EndEventTag`，未配默认 `Event.Combat.AttackEnd`）定 GA 结束位（`bStopWhenAbilityEnds=false`）→ ② 收刀段无主播放 + `AbilityWindow(State.Combat.Recovery)` → ③ 打断入口（`Move()`）查 tag → StopAnimMontage → **消费方显式 RemoveLooseGameplayTag**。**不调 CancelAbilities**（GA 由 OnInterrupted→EndAbility 兜底；`Move()` 内 Cancel 为 basic attack 旧残留，新技能不依赖）。
8. **5.8 API 事实**：`SetCustomTimeDilation` 已移除（直接赋属性）；`FGameplayModifierEvaluatedData` 构造必须 4 参；SetByCaller 用 FGameplayTag 版；UFUNCTION 参数禁止 struct 裸指针（用 GenericGameplayEventCallbacks + lambda）。
9. **文档同步与 git 提交时机**：流程 = 改完代码 → **人工 PIE 验证通过后** → 才同步文档 + 提交 git（验证前不动文档、不提交）；收尾 = 改对冲突文档节（来不及则节首盖 `⚠ 过期 YYYY-MM-DD`）+ 覆盖式更新「当前状态」（≤10 行、带日期；**只记进度/下一步/资产级待办**，细则进架构文档与 commit message，防段落回涨）同 commit。发现矛盾 → 以代码与最近定稿为准，禁按旧文档实现、禁静默忽略。

## MCP 资产操作规则

- **只读**：read / list / get_* / describe_*。
- **禁写**：set_property / save / create_* / duplicate / rename / delete / move / add_* / remove_* 一律禁止。BP 配置、Niagara、蒙太奇通知、资产创建由**人工在编辑器内操作**。
- 需要改资产时，向用户给出操作步骤指导（路径、面板、参数值），由用户执行。诊断 / 查日志 / PIE 验证不受限。

## 当前状态（2026-09-08 起压缩版——落地细节与坑已同步架构文档 + git commit，本节只留进度 / 待办 / 指针）

- ✅ Phase 1（GAS 基础设施）/ 1.5（输入→Tag 桥接）/ 2（普攻连段+双窗口输入缓冲+伤害管线+飘字）；敌人失衡恢复（`State.Stun` 由 Infinite 改 `UZZZGameplayEffect_Stun` Duration 5s 自过期——Daze 满不再永久卡死；判据改查 tag 而非 bool）——均 PIE 验证通过
- ✅ 普通切换 + 招架支援全链路 + 玩家俯仰限幅（-60/+30）——均 PIE 验证通过（2026-09-02/04/05）；**细节与坑（双段式蒙太奇、ParryImpact 事件路由、CDO ensure、快速支援预留口等）见设计文档 §4.9.2 与 §八**、`ZZZ-Camera-Architecture.md`、对应 commit；资产全配（Koleda），Jane 未配 Assist 会 warning
- ✅ 招架/突击 Motion Warping（2026-09-07，PIE 验证通过）——插件启用 + `UMotionWarpingComponent` 装配、`ZZZ_ParryLand`/`ZZZ_RushLand` 槽语义、SkewWarp 必达 target 算法事实、`a.MotionWarping.Debug` 调试，见设计文档与 commit 27059ab
- 🔄 Phase 3/3.5（闪避/完美闪避慢动作/震屏 HitStop/特殊技+能量/追击技/输入缓冲 Y 门控/编队·支援门控）：C++ 侧全部完成 ✅（档位模型、追击技=连段段、缓冲分流等定稿细节见架构文档 §4.13 与对应 commit）；剩**资产级待办**（人工在编辑器操作，不可从代码/git 恢复）：
  - GA_DashAttack / GA_Koleda_Counter 补 Asset Tags `Ability.Attack.Dash.Attack` / `.Counter`（现空 → Special DashLead 静默不触发；GA_DashAttack Blocked 残留 PerfectDodge 待清）
  - 其余角色 GA_SpecialAttack_* + 蒙太奇 + DefaultAbilities 追加（Jane 未配 Assist 会 warning）；TeamPanel；PIE 手测
- ⬜ Phase 4（元素异常）/ 5（终结技/连携技）/ 6（AI/关卡）
