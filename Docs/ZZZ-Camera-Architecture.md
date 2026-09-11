# 相机架构定稿 — Gameplay Cameras Manager 模式 + 切人过渡 / 特写镜头

> **项目**: ZZZCombatRemake (UE 5.8) · 定稿 2026-09-02 · 更新 2026-09-11
> **状态**: ✅ 切人平滑过渡已落地（manager 模式）；✅ 俯仰限幅落地（自定义子类 AZZZPlayerCameraManager，见 §八）；✅ **招架特写落地（2026-09-11 PIE 验证通过，见 §四）**；⬜ 大招分镜（Sequencer）基建已写、未验证（见 §四末）
> **依据**: UE 5.8 引擎源码 `Engine/Plugins/Cameras/GameplayCameras/`（逐行查证，非文档记忆）；参考方案（PlayerCameraManager 调度 + CineCamera/Sequence + CameraModifier）已在 5.8 语境下逐条修正

## 一、已确认决策

1. **插件**：Gameplay Cameras（引擎自带，`Engine/Plugins/Cameras/GameplayCameras/`，模块名 `GameplayCameras`）。
2. **运行模式：manager 模式（2026-09-02 定稿，推翻 08-31 的 standalone 初案）**：PC 构造函数设 `PlayerCameraManagerClass = AZZZPlayerCameraManager`（GameplayCameras manager 的自定义子类，2026-09-04 起带俯仰限幅，见 §八），组件 `bRunStandaloneCameraSystem = false`。
   - **推翻原因（实测）**：standalone 模式下每个角色组件跑**独立 camera system**，切人 = 换整个 system → blend 栈不存在 → 跨角色位置必瞬移，EnterTransitions 无机会执行。
   - manager 模式 = **单一 camera system**（manager 宿主）+ evaluation context 栈：切人时把目标成员 context 压栈 → 栈保留旧角色最后相机状态 → `EnterTransitions` 跨角色 blend 生效。
3. **切人过渡机制**：`OnPossess` → context 未激活时 `ActivateGameplayCamera(CameraComp, Push)` 压栈 → view target 随 `OnContextStackChanged` 自动同步 → `CA_PlayerCameras` 的 **`EnterTransitions`**（`UEasingBlendCameraNode`，0.25s HermiteCubicInOut，资产内配置）从旧角色最后一帧插值到新角色。
4. **角度连续**：两角色共用 `CR_ThirdPerson`（`BoomArmCameraNode` 无输入槽 → 引擎内部自动用 `UDrivenControlRotationCameraNode`）→ 旋转恒 = 玩家 ControlRotation。**任何代码不接管 ControlRotation**（`bSetControlRotationWhenViewTarget=false`）。
5. **特写镜头**（2026-09-11 落地，招架先行）：director（`CDE_PlayerCamera` BP）**每帧读 PC 的特写请求**决定 push 特写 rig 还是主 rig——**rig 不会自动归还，"归还"= 请求清空后 director 下一帧 push 主 rig**（引擎侧无超时/播完机制；初版此处"自带 ExitTransitions 自动归还"的说法有误，见 §四）；特写 rig 用 `SetLocation`/`SetRotation`（`Pawn` 空间）锁定机位与角度（BoomArm 无 InputSlot 会自动吃 player controller view rotation，特写不可用）；进阶形态（大招分镜）用 CineCameraActor + LevelSequence，见 §四末。
6. **震动/震屏**：沿用现有 `GC_ZZZ_CameraShake`（GameplayCue → `ClientStartCameraShake` + BP_HitShake），不引入 UCameraModifier（5.8 旧相机管线）。
7. **镜头穿透玩家/敌人**（2026-09-01）：玩家/敌人 capsule + mesh 对 `ECC_Camera` 显式 `ECR_Ignore`（`ZZZCharacter`/`ZZZCombatEnemy` 构造）——镜头只被世界几何遮挡，不被角色本体/敌人挡住。
8. **参考方案偏差修正**（5.8 语境）：
   - `SetViewTargetWithBlend` 旧参数由 `UViewTargetTransitionParamsBlendCameraNode` 桥接，但本项目过渡走资产内 Transition 配置（可视化、数据驱动、C++ 零参数）；
   - 5.8 **无** `DefaultRig`/`Follow`/`Orbit`/`LookAt`/`CineCameraRigRail`/`CameraRig_Blueprint`（已删/改名）；等效：`BoomArmCameraNode`、`DampenPosition/RotationCameraNode`、`UBlueprintCameraNode`；
   - blend 时长单位 = **秒**，曲线枚举 = `EEasingCameraBlendType`。

## 二、架构分层

```
控制层  ZZZPlayerController（调度，C++）
         ├─ 构造: PlayerCameraManagerClass = AZZZPlayerCameraManager（俯仰限幅子类）
         ├─ OnPossess（唯一入口, 覆盖初始 Possess + 每次切人）:
         │    GetEvaluationContext() 未激活 → manager->ActivateGameplayCamera(Push)
         │    （已激活 = 切回成员 → 跳过, 由 AutoManage→SetViewTarget 移顶, silent）
         ├─ 切人: SwitchToNextCharacter（位置/朝向对齐 → 保存 ControlRotation
         │    → Possess → 恢复 ControlRotation → 异步退场状态机）
         └─ 特写(预留): manager/组件 ActivateCameraRig 切特写 rig
数据层  /Game/ZZZ/Camera/
         ├─ CA_PlayerCameras    CameraAsset（director = CDE_PlayerCamera:
         │                      BlueprintCameraDirectorEvaluator）
         │    └─ EnterTransitions: UEasingBlendCameraNode（0.25s, HermiteCubicInOut）
         ├─ CR_ThirdPerson      主 rig（Array → BoomArm + FOV + CollisionPush；
         │                      玩家 ControlRotation 驱动 → 角度天然连续）
         └─ CR_Closeup_*        特写 rig（后续新建；rig 自带 Enter/ExitTransitions）
表现层  现有 GC_ZZZ_CameraShake（GameplayCue → ClientStartCameraShake）——不动
```

职责划分：**控制层**决定"用哪个角色的相机/哪个特写"，**数据层**（资产）决定"镜头长什么样、怎么过渡"，**表现层**（Cue/Shake）瞬时反馈。相机计算全在 rig 求值管线（非 Actor Tick）。

## 三、切人过渡实现（已落地，2026-09-02 验证）

### 机制（引擎源码确认）

1. **相机宿主** = `AGameplayCamerasPlayerCameraManager`：单一 camera system + evaluation context 栈（`FCameraEvaluationContextStack`，top = active）。
2. **组件职责**（`bRunStandaloneCameraSystem=false`）：每角色只提供 evaluation context（引用 `CA_PlayerCameras`）；`BeginPlay` auto-activate 调 `ActivateCameraForPlayerController(nullptr, false)`——只建 context **不设 view target**。
3. **C++ 入口 = `AZZZPlayerController::OnPossess`**：
   - context 未激活 → `ActivateGameplayCamera(CameraComp, Push)` → context 压栈激活 → `OnContextStackChanged` 把 view target 同步为该角色 ✅
   - context 已激活（切回已入栈成员）→ **不调**（重复 Push 会 log error）；切回由 `bAutoManageActiveCameraTarget`（PC 默认 true）→ `SetViewTarget` 处理：manager 在栈中按 owner 找到既有 context 并**移顶**（`PushContext` 幂等，无错）
   - ⚠ 不走 `PC::SetViewTarget` 直达：manager 的 SetViewTarget 对无 `UCameraComponent` 的 Pawn 只建 **actor-copy context**（复制 actor 属性的程序化 rig → 相机卡在 Pawn 原点，实测踩坑）
4. view target 变更 → 新 context 的 rig 激活 → **`EnterTransitions` blend**（从栈上旧角色最后一帧状态插值）；旋转 = ControlRotation 不变。
5. **Possess 重置 ControlRotation 的坑**：`APlayerController::Possess → OnPossess` 内部 `SetControlRotation(新Pawn->GetActorRotation())`。切人时 `SwitchToNextCharacter` 在 Possess 前保存、后恢复玩家 ControlRotation（否则镜头被拉向角色移动朝向、俯仰被清空）。

### C++ 改动清单（全部已落地）

- `ZZZCombatRemake.Build.cs`：`PublicDependencyModuleNames` + `"GameplayCameras"`
- `ZZZ/Player/ZZZPlayerCameraManager`（新增 2026-09-04）：`AGameplayCamerasPlayerCameraManager` 自定义子类，构造内 `ViewPitchMin=-60 / ViewPitchMax=30`（俯仰限幅，见 §八）
- `ZZZPlayerController` 构造函数：`PlayerCameraManagerClass = AZZZPlayerCameraManager::StaticClass()`（OnPossess 内 Cast 仍用基类 `AGameplayCamerasPlayerCameraManager`，子类 IS-A 兼容）
- `ZZZPlayerController::OnPossess` override：幂等激活当前角色相机（见上）
- `SwitchToNextCharacter`：`SetActorLocationAndRotation`（继承旧角色位置+朝向）+ 保存/恢复 ControlRotation；相机激活不再在此处（OnPossess 统一）
- `ZZZCharacter` / `ZZZCombatEnemy` 构造：capsule + mesh 对 `ECC_Camera` Ignore

### 资产配置（用户操作，MCP 禁写资产）

1. 两角色 BP（BP_Okuma / BP_Jane）`GameplayCamera` 组件：**`bRunStandaloneCameraSystem` = false**（已做）。
2. `CA_PlayerCameras` Enter Transitions：1 条无条件 transition，Blend = `UEasingBlendCameraNode`（`BlendTime=0.25` 秒，`BlendType=HermiteCubicInOut`）（已做）。
3. `InitialOrientation` 默认 None；若实测 blend 期间角度被拉向旧朝向才改 `PreviousYawPitch`（未触发）。

## 四、特写镜头（招架已落地 2026-09-11 PIE 验证；连携/大招复用本基建）

### 机制（链路三层，职责分离）

1. **请求（C++，只写状态不切镜头）**：`UZZZAssistDefensive::StartCloseupCamera()`（`ActivateAbility` 蒙太奇起播后 = **按下空格瞬间**，PC 的 TryParrySwitch 是按键帧内同步链）→ `AZZZPlayerController::RequestCloseupCamera(UCameraRigAsset*)`（PC 上 Transient 请求位）。
2. **执行（BP director，唯一激活点）**：`CDE_PlayerCamera` 每帧 `RunCameraDirector` 读 `GetRequestedCloseupRig()`——非空 → `ActivateCameraRig(特写 rig)`；空 → `ActivateCameraRig(CR_ThirdPerson)`。**为什么必须过 director**：它每帧的 push 会顶掉任何绕路的 push（`TransientBlendStackCameraNode::Push` 去重只对"栈顶同 context 同 rig"生效，`TransientBlendStackCameraNode.cpp:28-78`）。
3. **归还（同 ②）**：`CloseupDuration`（默认 1.2s，**按键起算**，需覆盖 最坏入场余量≈0.66s + 定格 0.3s + 收势）+ GA 结束兜底 → PC 清请求 → director 下一帧 push 主 rig → 特写被 blend out + pop。
   ⚠ **引擎无"rig 自动归还"**（初版本节"自带 ExitTransitions 自动归还"的说法有误）：`ExitTransitions` 只是 blend 资产，触发条件只有"被新 push 顶掉"或显式 deactivate。

### 机位锁定（特写 rig 搭法）

- `SetLocationCameraNode`（机位）+ `SetRotationCameraNode`（角度），两者 **`OffsetSpace = Pawn`** → 相对角色固定、**完全不跟鼠标**。
- **不要用 BoomArm**：无 `InputSlot` 时引擎自动吃 player controller view rotation（`BoomArmCameraNode.h:107-112` 注释明说），机位与角度都会跟鼠标。
- **CollisionPush 删掉**：特写机位离角色仅 2-3m，贴墙时避障推镜头会让构图乱跳。
- `FieldOfView` 收小（CR_Closeup_Parry 用 38）出长焦压缩感。

### 过渡速度（切入/切出分别可调）

- **查找顺序**（`TransientBlendStackCameraNode.cpp:584-660`，按序取**第一条条件匹配**）：
  `① 旧rig Exit → ② 旧asset Exit → ③ 新rig Enter → ④ 新asset Enter`。
- `CR_Closeup_Parry` 的 **Enter** 配 Easing **0.06s**（快切）；**Exit** 可不配（fallback 到 CA 的 0.25s 平滑归还）。
- ⚠ **`CR_ThirdPerson` 不要配过渡**："切人"与"切入特写"的 **From rig 都是它**（切人 = 角色 A 主 rig → 角色 B 主 rig，rig 资产同一个），配了会同时误伤两种切换，且两者需求速度不同无法用一条无条件过渡区分。
- **CA_PlayerCameras 现有那条 0.25s 保留**（切人本职 + 全局兜底）。
- **编辑入口**：rig 的 Enter/ExitTransitions 是 `UPROPERTY(Instanced)` 无 `EditAnywhere`（`CameraRigAsset.h:82-84`）→ **不在细节面板**；入口 = Camera Rig 编辑器**工具栏的 `Transitions` 按钮**（与 `Node Hierarchy` 并列，`CameraRigAssetEditorCommands.cpp:32-35`），切到过渡图（水印 TRANSITIONS）编辑。
- Transition 结构 = `Conditions[]`（留空=无条件；可用 `Is Camera Rig` 限 Previous/Next rig，或 `Gameplay Tag` 查 rig 的 GameplayTags）+ `Blend`（Easing/Pop/Linear…）；顺序敏感（取第一条全条件通过的）。

### 本轮未做 / 后续

- **输入屏蔽**：未做（招架期按键已被 GA 吞；Look 未锁，PIE 观感可接受）。
- **参数化机位**：未做（机位写死在 rig 节点；CameraAsset 参数通道 `ParameterDefinitions` + `DefaultParameters` PropertyBag 保留待用）。
- **瞬时反馈**：震屏沿用既有 `GC_ZZZ_CameraShake`，特写自身无 shake。
- **大招分镜（⬜ 基建已写、未 PIE 验证）**：`AZZZPlayerController::PlayCinematic(ULevelSequence*, AActor*, FName)` / `StopCinematic()` / `IsCinematicPlaying()` + 控制台测试命令 `ZZZ.PlayCinematic`；走 CameraCut 标准路径（CameraCut → `PC->SetViewTarget(CineCameraActor)` → manager 建 `FActorCameraEvaluationContext` 压栈；归还配置在 **CameraCut section 的 When Finished = Restore State**——5.8 的 `FMovieSceneSequencePlaybackSettings` **无 `bRestoreState` 字段**，旧资料误导）。正式形态：大招 pose 阶段（无伤害）Sequence 全包（相机+动画+遮挡），释放阶段归还主 rig 走 GA 蒙太奇。

## 五、各角色差异化视角

- `CA_PlayerCameras` 的 `ParameterDefinitions` 定义可调参数（臂长/偏移/FOV）；
- 组件 `CameraReference.Parameters`（`FCameraAssetReference::Parameters` FInstancedPropertyBag）按角色覆盖——无需每角色独立 asset。

## 六、风险与坑（实测记录）

1. **重复激活报错**：`ActivateGameplayCamera` 对已激活 context 会 log error（`its evaluation context is already active`）——**幂等规则**：只在 context 未激活时 Push；切回由 AutoManage→SetViewTarget 移顶（silent）。已修复（2026-09-02，OnPossess 内查 `IsActive()`）。
2. **manager SetViewTarget 的 actor-copy 陷阱**：view target 是无 `UCameraComponent` 的 Pawn 且栈中无其 context 时，manager 建"复制 actor 属性"的程序化 context → 相机卡 Pawn 原点。**必须走 `ActivateGameplayCamera` 先入栈**。
3. **standalone 无法跨角色 blend**（推翻初案的实测依据）：每组件独立 system，切换 = system 更换 = camera cut。勿回退 standalone。
4. **Possess 重置 ControlRotation**：保存/恢复见 §三.5。
5. **`InitialResult isn't valid` 警告**：首次激活帧 context 尚未被组件 Tick 更新 → blend stack 一次性警告（`bLogWarnings` 只打一次），无害。
6. **编辑器 ensure 崩溃**（`CameraObjectGraphSchemaBase.cpp:559`）：在相机图空白处触发"New Interface Parameter"类右键操作会崩（引擎 UI 缺陷）——**配置 EnterTransitions 走专门的过渡编辑 UI**，勿在图空白右键建节点。
7. **CollisionPush 抖动**：不配 Push/Pull 插值器时引擎默认 `TPopValueInterpolator`（瞬跳）→ 边界振荡剧烈抖动。修复 = CollisionPush 节点详情面板**内联创建** Critical Damper（`UCameraValueInterpolator` 是 `EditInlineNew` 内联类，**非独立资产**，内容浏览器无法创建——在节点属性槽上点「+」新建，PushInterpolator `DampingFactor=1.0`、PullInterpolator `1.2`）+ 调小 `CollisionSphereRadius`。
8. **角度连续性**：blend 两端旋转均 ControlRotation，无感（未触发 `PreviousYawPitch` 兜底）。
9. **切人节奏**：0.25s 过渡期内输入已切到新角色；连切由 `Started` 触发 + `bIsSwitching` 守卫防抖。
10. **特写期间移动基准**（2026-09-11 落地结论）：DoMove 读 ControlRotation Yaw，特写镜头角度与它分离——不锁 Look 也不影响观感：招架特写期间玩家在播蒙太奇（按键被 GA 吞），移动输入无从生效；特写结束归还主 rig 即回 ControlRotation 方向。连携/大招若允许特写中操作，再评估 `SetIgnoreLookInput`。
11. **rig 过渡不在细节面板**：`EnterTransitions`/`ExitTransitions` 是 `UPROPERTY(Instanced)` 无 `EditAnywhere`（`CameraRigAsset.h:82-84`）——找它去 **Camera Rig 编辑器工具栏的 `Transitions` 按钮**（与 `Node Hierarchy` 并列），**不在节点图、不在细节面板**（2026-09-11 实查）。
12. **给 `CR_ThirdPerson` 配过渡会误伤**：切人（主 rig→主 rig）与切入特写（主 rig→特写 rig）的 **From rig 都是它**——配无条件过渡会同时命中两种、且两者需求速度不同；保持空（详见 §四·过渡速度）。
13. **BoomArm 自动吃 ControlRotation**：`InputSlot` 为空时引擎自动接 player controller view rotation（`BoomArmCameraNode.h:107-112`）——主 rig 依赖此行为（旋转恒=ControlRotation），特写 rig 必须换 `SetLocation`/`SetRotation`（`Pawn` 空间）。
14. **特写归还必须显式**：引擎无"rig 播完自动归还"；本项目的实现 = director 每帧读 PC 请求（清空即归还）——初版"ExitTransitions 自动归还"的说法已作废。

## 七、关键文件

- `ZZZ/Player/ZZZPlayerController`（构造函数 manager 指定 + `OnPossess` 幂等激活 + `SwitchToNextCharacter` + **特写请求通道 / `PlayCinematic` 分镜播放入口**，2026-09-11）
- `ZZZ/Player/ZZZPlayerCameraManager`（2026-09-04 新增，俯仰限幅，见 §八）
- `ZZZ/Abilities/ZZZAssistDefensive`（**招架特写触发**：`CloseupRig`/`CloseupDuration` + `StartCloseupCamera`/`StopCloseupCamera`，2026-09-11）
- `ZZZ/ZZZCharacter` / `ZZZ/Enemies/ZZZCombatEnemy`（胶囊/mesh 对 ECC_Camera Ignore）
- `ZZZCombatRemake.Build.cs`（`GameplayCameras` + `LevelSequence`/`MovieScene` 依赖）
- `/Game/ZZZ/Camera/`：`CA_PlayerCameras`（asset + EnterTransitions）、`CR_ThirdPerson`（主 rig）、`CDE_PlayerCamera`（director BP：每帧读特写请求 → push 特写/主 rig）、**`CR_Closeup_Parry`**（招架特写 rig：SetLocation/SetRotation[Pawn] + FOV 38 + Enter Easing）
- 角色 BP：`/Game/ZZZ/character/BP_Okuma`、`BP_Jane`（GameplayCameraComponent：standalone=false，已配）
- 表现层：`ZZZ/Cues/GC_ZZZ_CameraShake`（不动）

## 八、玩家俯仰限幅（自定义子类，2026-09-04 落地）

**需求**：相机俯仰包络 [-60°, +30°]（经典 ViewPitchMin/Max 写法，向下/向上）。

**实现**：新增 `ZZZ/Player/ZZZPlayerCameraManager` —— `AGameplayCamerasPlayerCameraManager` 自定义子类（引擎类 `UCLASS(notplaceable, MinimalAPI)`，构造函数单独导出，C++ 继承完全支持），构造内设 `ViewPitchMin = -60.0f` / `ViewPitchMax = 30.0f`；PC 构造函数 `PlayerCameraManagerClass` 指向子类（旧行 `Cast<AGameplayCamerasPlayerCameraManager>` 不变，子类 IS-A 兼容）。BP 资产零改动。

**5.8 生效点（引擎源码逐行核实，勿按旧管线认知改动）**：

1. `ViewPitchMin/Max` 的唯一按帧消费点 = `APlayerCameraManager::ProcessViewRotation → LimitViewPitch`（钳 ControlRotation）；调用链 `APlayerController::UpdateRotation` 每帧：取 ControlRotation → ProcessViewRotation → SetControlRotation。
2. GameplayCameras manager 的 `DoUpdateCamera` **刻意不调 Super**（rig 输出 `GetEvaluatedCameraView` → ApplyCameraModifiers → FillCameraCache），旧 `UpdateViewTarget` 的整条 POV 处理/限幅已不存在 → **限的是 ControlRotation，不是 rig 输出 POV**。
3. 本项目 CR_ThirdPerson 旋转恒 = ControlRotation（BoomArm 无输入槽 → 引擎自动接 Driven Control Rotation 节点；该节点写回前同样读 manager 的 `ViewPitch*` 限值，`FDrivenControlRotationCameraNodeEvaluator::LimitControlRotation`）→ 钳 ControlRotation = 钳最终相机俯仰，等效成立。
4. 边界：rig 内自累积旋转节点（orbit 自加输入）与跨限位长 blend 的输出不经任何限幅——若未来引入此类节点，改在 rig 图内钳制，本子类不再兜底。VR 头显（IsHeadTrackingAllowed）时引擎跳过限幅。

**语义**：限幅作用于 ControlRotation → 相机与瞄准（FaceRotation/aim）共用同一俯仰包络——相机=瞄准为本作既有定稿，属预期。
