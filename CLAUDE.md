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
9. **文档同步与 git 提交时机**：流程 = 改完代码 → **人工 PIE 验证通过后** → 才同步文档 + 提交 git（验证前不动文档、不提交）；收尾 = 改对冲突文档节（来不及则节首盖 `⚠ 过期 YYYY-MM-DD`）+ 覆盖式更新 `Docs/ZZZ-Combat-Phase3-Plan.md` 顶部状态行（≤10 行、带日期；**只记进度/下一步/资产级待办**，细则进架构文档与 commit message，防段落回涨）同 commit。发现矛盾 → 以代码与最近定稿为准，禁按旧文档实现、禁静默忽略。

## MCP 资产操作规则

- **只读**：read / list / get_* / describe_*。
- **禁写**：set_property / save / create_* / duplicate / rename / delete / move / add_* / remove_* 一律禁止。BP 配置、Niagara、蒙太奇通知、资产创建由**人工在编辑器内操作**。
- 需要改资产时，向用户给出操作步骤指导（路径、面板、参数值），由用户执行。诊断 / 查日志 / PIE 验证不受限。

## 当前进度（指针，勿在本文件维护副本）

- 进度、下一步、资产级待办 → `Docs/ZZZ-Combat-Phase3-Plan.md` 顶部状态行（规则 9 收尾同步目标）；全 Phase 总览 → System-Design §五 路线图；相机进度 → Camera 文档状态行。**会话开工先读 Phase3-Plan 状态行。**
- 已 PIE 验证功能的落地细节与坑 → System-Design 对应节 + git commit，不在本文件复述。

