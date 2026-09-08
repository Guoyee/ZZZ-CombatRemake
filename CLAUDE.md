# CLAUDE.md

UE 5.8 单机 C++ 项目，复刻《绝区零》(Zenless Zone Zero) 核心战斗逻辑，GAS（Gameplay Ability System）为架构核心。

## 必读文档

- `Docs/ZZZ-Combat-System-Design.md` — 唯一权威架构设计，开发前必读
- `Docs/ZZZ-Combat-Phase3-Plan.md` — 当前实施进度与待办（Phase 3 进行中）
- `Docs/ZZZ-Camera-Architecture.md` — 相机架构定稿（GameplayCameras manager 模式 + 切人过渡 + 特写预留），开发前必读
- `Docs/archive/` — 旧版完整文档备份，**勿读**（仅人工查证历史时使用）

## 构建

```bash
# 生成 VS 工程文件
"D:/Program Files/Epic Games/UE_5.8/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe" -ProjectFiles -Project="D:/UE5/ZZZCombatRemake/ZZZCombatRemake.uproject" -Game -Engine="D:/Program Files/Epic Games/UE_5.8"

# 构建编辑器目标（新增 UCLASS / UPROPERTY 后必须完整构建，UHT 重跑）
"D:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" ZZZCombatRemakeEditor Win64 Development -Project="D:/UE5/ZZZCombatRemake/ZZZCombatRemake.uproject"

# 打开项目
"D:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "D:/UE5/ZZZCombatRemake/ZZZCombatRemake.uproject"
```

小改动可用 Live Coding；每个实施 Task 完成即构建 + PIE 手测，不跨 Task 堆积。

## 架构速览

- 单模块 `Source/ZZZCombatRemake/`，依赖 EnhancedInput / AIModule / StateTreeModule / GameplayStateTreeModule / UMG / GameplayAbilities。
- `ZZZ/` 是目标模块（GAS 战斗）；`Variant_*` 是模板参考代码，**勿改**。
- 角色 = BP 子类 + 每角色自己的 GA BP 资产（`DefaultAbilities`）；不建 per-character C++ 类。输入配置（`UZZZInputConfig`，IA→GameplayTag）全局共享。
- 核心类：`AZZZCharacter`（Pawn-ASC）/ `AZZZCombatEnemy` / `UZZZAttributeSet` / `UZZZGameplayAbility` 及子类 / `UZZZDamageExecution`（统一 ExecCalc：HP+Daze+Anomaly）。
- 命名：`Elimination` 非 `Kill`；属性 `Health` 非 `CurrentHP`；GameplayCue 加 `ZZZ_` 前缀。
- UE 5.8 官方文档：https://docs.unrealengine.com/5.8/en-US/ （API 疑问优先查官方）

## 硬性规则

1. **Tag 分层管理，按场景选机制**（2026-08-29 定稿，替代 2026-08-10 版）：
   - **A. 动画窗口**（notify Begin/End 配对、仅本地 ASC 查询消费）→ **允许 notify 配对 `AddLooseGameplayTag`/`RemoveLooseGameplayTag`**（notify 共享实例下最简洁，无需 per-owner 句柄映射；单机无网络同步缺陷）。Tag：CanCombo / CanBuffer / CanDashAttack / Enemy.AttackWindow / State.Combat.Recovery / State.PassThrough（Collision Pass-Through notify 授予，RotateToTarget 停止索敌转向）。⚠ 打断可能不触发 NotifyEnd → **双轨兜底**：① 有主段（GA 存活）由消费该 tag 的能力 **EndAbility 兜底移除**；② 无主段（GA 已结束、收刀/起身段无主播放）由消费方（如 `Move()`）**显式清理**（`RemoveLooseGameplayTag`）。**窗口 GE 化已取消**（2026-08-29 定稿：LooseTag 为 A 层方案）。禁止 notify 之外随手 Add。
   - **B. 持久身份**（无生命周期所有者：State.Player/Enemy/Alive）→ Infinite GE + 应用时 `DynamicGrantedTags`（`UZZZGameplayEffect_Faction`/`_Alive`）。
   - **C. 核心状态**（被多系统读取、参与死亡/失衡判定：State.Dead/Stun/Staggered）→ **GE 管理（禁 LooseTag）**：`UZZZGameplayEffect_Dead`（Infinite）/`_Stun`（Duration 5.0s 自过期，2026-09-05 起——曾为 Infinite 致 Daze 满后永久卡死，PIE 复现确认）/`_Stagger`（Duration 0.35s 自过期），见 `ZZZ/Effects/ZZZStatusGameplayEffects.h`。
   - **D. 能力生命周期**（随能力激活/结束，含打断路径：State.Attacking/Invulnerable）→ `ActivationOwnedTags`。
   - **E. 本地路由标志**（能力内部短暂标志：State.PerfectDodge）→ Duration GE 自过期（无 Timer 无句柄管理）。
   - 废弃 tag：`Effect.Ability.CanDodge`（完美闪避判定前移 2026-08-09 后无消费方）、`Effect.Ability.CanParry`（弹刀与极限闪避共用 `Enemy.AttackWindow`，设计 §4.9）——保留注册防旧资产报错，新开发勿用。
   - C++ 载体 vs BP 资产：系统级零参数 GE（B/C 层，调用点在 AttributeSet/Enemy）用 C++ 载体类；能力级可调 GE（E 层）用 BP 资产 + `TSubclassOf` 配置。
2. **GE CDO 时序**：GE CDO 在模块加载期构造（早于 StartupModule），构造函数内 `FZZZGameplayTags::Get()` 无效，配进 CDO 的 Tag 会静默丢弃 → Tag 一律**应用时**解析。构造函数内子对象用 `CreateDefaultSubobject`，禁用 `NewObject`（Fatal）。
3. **UE 5.3+ GE 组件**：`GrantedTags` / `Application Tag Requirements` 等 GE 直接属性已废弃，用 `GEComponents`（`UTargetTagsGameplayEffectComponent` 授予 / `UTargetTagRequirementsGameplayEffectComponent` 过滤）。
4. **GCN 注册表镜像（R20）**：CueManager 扫描只读 FAssetData 的 `GameplayCueName`，引擎派生逻辑会把它擦成 None → unmapped 静默丢弃。GCN 必须重写 `PostInitProperties`/`PostLoad`/`Serialize` 同步 `GameplayCueName = GameplayCueTag.GetTagName()`；`IsOverride = true`；cue tag 需 ini 注册（`+Prop=` 逐元素语法）。
5. **视觉反馈走 GameplayCue**：禁止在 `PostGameplayEffectExecute` 中 SpawnEmitter，走 `ExecuteGameplayCueOnActor`。
6. **M1 回调区分**：`PostGameplayEffectExecute` 仅对 Instant GE 触发；Duration/Infinite GE 属性变化走 `GetGameplayAttributeValueChangeDelegate()`。
7. **收刀 = 连段窗口 + 标准打断流程**：单 Montage 双 Section（Attack + Recovery）；连段过渡**先激活下一段再 EndAbility**（防 BlendOut→BlendIn 空窗）；收刀段全程 Root Motion。**收刀打断标准流程（2026-08-29 定稿，新技能一律照此）**：① 动作段末 `AnimNotify_SendGameplayEvent(EndEventTag)` **决定 GA 结束位置**（GA 提前 EndAbility，`bStopWhenAbilityEnds=false`；`EndEventTag` 未配置时默认 `Event.Combat.AttackEnd`（2026-08-29，基类惰性解析，GA_Dodge 覆盖为 `DodgeEnd`））→ ② 收刀段**无主播放** + 挂 `AbilityWindow(State.Combat.Recovery)` 可打断 tag → ③ 打断入口（`Move()`）**查询 tag → StopAnimMontage → 消费方显式清除 tag**（`RemoveLooseGameplayTag`；无主段 EndAbility 兜底不可达——GA 已结束，见规则 1 A 层双轨②）。**不调用 CancelAbilities**——GA 由蒙太奇中断回调 OnInterrupted → EndAbility 覆盖（2026-08-29 定稿）；现 `Move()` 中的 Cancel 为旧方式残留（现有 basic attack 依赖），新技能不依赖。
8. **5.8 API 事实**：`SetCustomTimeDilation` 已移除（直接赋 `CustomTimeDilation` 属性）；`FGameplayModifierEvaluatedData` 构造必须 4 参；SetByCaller 用 FGameplayTag 版；UFUNCTION 参数禁止 struct 裸指针（`const FGameplayEventData*` 会 UHT 报错，用 GenericGameplayEventCallbacks + lambda）。

9. **文档同步与 git 提交时机（2026-09-02 定稿，取代曾试行的分层/戳/状态文件方案）**：流程 = 修改完代码 → **人工 PIE 验证通过后** → 才同步文档 + 提交 git（验证前不动文档、不提交，避免无效的文档修改和提交）；收尾动作 = 把与代码冲突的文档节改对（来不及则节首盖 `⚠ 过期 YYYY-MM-DD`）+ 覆盖式更新下方「当前状态」（≤10 行、带日期；**只记进度 / 下一步 / 资产级待办**——定稿细节与踩坑进架构文档和 commit message，禁止写回该段，防段落回涨），与代码同 commit。发现文档与代码 / 较新定稿矛盾 → 以代码和最近定稿为准，禁止按旧文档实现、禁止静默忽略。

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
