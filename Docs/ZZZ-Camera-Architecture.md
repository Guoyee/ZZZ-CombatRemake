# 相机架构定案 — Gameplay Cameras 分层 + 切人过渡 / 特写预留

> **项目**: ZZZCombatRemake (UE 5.8) · 定稿 2026-08-31
> **状态**: 架构定案 —— 切人平滑过渡实施中；特写/输入屏蔽为预留
> **依据**: UE 5.8 引擎源码 `Engine/Plugins/Cameras/GameplayCameras/`（非文档记忆）；参考方案（PlayerCameraManager 调度 + CineCamera/Sequence + CameraModifier）已在 5.8 语境下逐条修正

## 一、已确认决策

1. **插件**：Gameplay Cameras（引擎自带，`Engine/Plugins/Cameras/GameplayCameras/`，模块名 `GameplayCameras`）。5.8 中插件已移出 Experimental，默认可用。
2. **运行模式**：**standalone 组件模式**（`UGameplayCameraComponent`，`bRunStandaloneCameraSystem=true`），不迁移 `AGameplayCamerasPlayerCameraManager`。理由：组件 API 已覆盖全部所需（激活/去激活、`ActivatePersistentBase/Global/VisualCameraRig`、camera actions、camera shake asset）；迁移仅在需要**全局多 rig 堆叠/优先级**时触发（届时 PC 的 `PlayerCameraManagerClass` 指定 `AGameplayCamerasPlayerCameraManager`，评估其 `ActivateGameplayCamera(Push)` 压栈语义）。
3. **切人过渡** = `SetViewTarget` 到新角色时触发 `CA_PlayerCameras` 的 **`EnterTransitions`**（`UCameraRigTransition` + `UEasingBlendCameraNode`），时长/曲线全部资产内配置，**C++ 只负责激活组件**（零相机参数）。
4. **角度连续**：两个角色共用 `CR_ThirdPerson`（`BoomArmCameraNode` 读玩家 ControlRotation）→ blend 两端旋转一致，**角度始终由玩家鼠标控制，任何代码不接管 ControlRotation**（`bSetControlRotationWhenViewTarget=false` 已确认）。
5. **特写镜头**（弹反/连携）：director（`CDE_PlayerCamera` BP）内 `ActivateCameraRig(特写Rig)` 切换，特写 rig 自带 Enter/ExitTransitions 自动归还主 rig；进阶形态（固定路径演出，Phase 5 ChainCamera 评估）用 CineCameraActor + LevelSequence + 末帧 POV 对齐归还。
6. **震动/震屏**：沿用现有 `GC_ZZZ_CameraShake`（GameplayCue → `ClientStartCameraShake` + BP_HitShake），**不引入 UCameraModifier**（5.8 旧相机管线，无必要）。
7. **参考方案偏差修正**（5.8 语境）：
   - `SetViewTargetWithBlend` 的旧参数被 `UViewTargetTransitionParamsBlendCameraNode` 桥接，但本项目过渡走资产内 Transition 配置（可视化、数据驱动、C++ 零参数）；
   - 5.8 **无** `DefaultRig` / `Follow` / `Orbit` / `LookAt` / `CineCameraRigRail` / `CameraRig_Blueprint` 节点（已删/改名）——等效能力：`BoomArmCameraNode`（臂+玩家旋转）、`DampenPosition/RotationCameraNode`（平滑）、`UBlueprintCameraNode`（节点级 BP 逻辑，替代 CameraRig_Blueprint）；
   - blend 时长单位 = **秒**（`UEasingBlendCameraNode::BlendTime` float），曲线枚举 = `EEasingCameraBlendType`（HermiteCubicInOut 等）。

## 二、架构分层

```
控制层  ZZZPlayerController（调度，C++）
         ├─ 切人: SwitchToNextCharacter → 激活新成员 GameplayCameraComponent
         │    （ActivateCameraForPlayerController → 内部 SetViewTarget
         │      → CA_PlayerCameras.EnterTransitions 自动 blend）
         ├─ 特写(预留): director/组件 ActivateCameraRig 切特写 rig
         │    （ActivatePersistentVisualCameraRig / DeactivateCameraRig）
         └─ 输入(预留): 特写活跃期间 SetIgnoreLookInput（项目现无先例，需新写）
数据层  /Game/ZZZ/Camera/
         ├─ CA_PlayerCameras    CameraAsset（director = CDE_PlayerCamera:
         │                      BlueprintCameraDirectorEvaluator）
         │    └─ EnterTransitions: UEasingBlendCameraNode（0.25s, HermiteCubicInOut）
         ├─ CR_ThirdPerson      主 rig（Array → BoomArm + FOV；
         │                      玩家 ControlRotation 驱动 → 角度天然连续）
         └─ CR_Closeup_*        特写 rig（后续新建；rig 自带 Enter/ExitTransitions）
表现层  现有 GC_ZZZ_CameraShake（GameplayCue → ClientStartCameraShake）——不动
```

职责划分：**控制层**决定"用哪个角色的相机/哪个特写"，**数据层**（资产）决定"镜头长什么样、怎么过渡"，**表现层**（Cue/Shake）负责瞬时反馈。相机计算全部发生在 rig 求值管线（非 Actor Tick），天然符合"避免 Tick、消灭跳变"最佳实践。

## 三、切人过渡实现

### 机制推演

1. 每个角色 BP（BP_Okuma / BP_Jane）挂 `GameplayCameraComponent`（`bRunStandaloneCameraSystem=true`、`DefaultPlayer=Player0`、`bAutoActivate=true`），引用 `CA_PlayerCameras`。
2. 新 spawn 成员：`BeginPlay` 自动激活 → `SetViewTarget(自己)` → 相机系统接管。
3. **已存在成员（切回）**：组件早已激活，view target 仍指向当前角色 → 必须**显式重新激活**抢回 view target。
4. `SetViewTarget` 到新角色 → 相机系统为 view target 推入新 evaluation context → 新 asset 的 **`EnterTransitions` 自动执行 blend**（从旧角色最后一帧相机状态插值到新角色）。

### C++ 介入点（仅 1 处）

`AZZZPlayerController::SwitchToNextCharacter()` — `Possess(NextMember)` 之后：

```cpp
if (UGameplayCameraComponent* CameraComp =
        NextMember->FindComponentByClass<UGameplayCameraComponent>())
{
    CameraComp->ActivateCameraForPlayerController(this);
}
```

- API：`UGameplayCameraComponentBase::ActivateCameraForPlayerController(APlayerController*, bool bSetAsViewTarget = true)`（`GameFramework/GameplayCameraComponentBase.h`，BlueprintCallable）；include `GameFramework/GameplayCameraComponent.h`。
- 幂等：已激活组件重复调用 = 重新 SetViewTarget 自己的 owner actor，语义安全。
- 模块依赖：`ZZZCombatRemake.Build.cs` PublicDependency 加 `"GameplayCameras"`。

### 资产配置（用户手动，MCP 禁写资产）

1. 编辑器打开 `CA_PlayerCameras` → Camera Asset 编辑器 → **Enter Transitions** 添加一条 transition（条件默认：无条件）。
2. Transition 的 Blend = **`UEasingBlendCameraNode`**：`BlendTime = 0.25`（秒）、`BlendType = HermiteCubicInOut`。
3. `InitialOrientation` 默认 None（blend 起点取上一相机状态）；若实测 blend 期间角度被拉向旧朝向，改 `PreviousYawPitch`。
4. 若 asset 级 EnterTransitions 实测未生效（仍瞬切），降级到 `CR_ThirdPerson` rig 级 EnterTransitions（同参数）。

## 四、特写镜头预留（弹反 / 连携，后续实施）

- **入口**：`CDE_PlayerCamera`（`UBlueprintCameraDirectorEvaluator` 子类）BP 内重写 `RunCameraDirector` / 调 `ActivateCameraRig(特写Rig, bForceNewInstance)`；或组件侧 `ActivatePersistentVisualCameraRig` / `DeactivateCameraRig(InstanceID)`。
- **归还**：特写 rig 自带 EnterTransitions（blend in）/ ExitTransitions（blend out），过渡结束自动 blend 回主 rig——无需手动"归还"，避免跳变。
- **参数**：目标位置/时长等走 CameraAsset 参数（`ParameterDefinitions` + `DefaultParameters` PropertyBag），由 GAS 能力/GC 或 director BP 传入。
- **输入屏蔽**：特写活跃期间 PC `SetIgnoreLookInput(true)`（项目无先例，落地时新写；配合 StateTree/GA 生命周期）。
- **进阶形态**（Phase 5 ChainCamera 评估）：CineCameraActor + LevelSequence，Sequence 末帧将 `APlayerCameraManager` POV 与 Sequence 相机对齐后交还控制权。
- **瞬时反馈**（FOV 冲击/径向模糊）：优先在特写 rig 内做（数据驱动）；或 CameraShake 资产（现有管线）。

## 五、各角色差异化视角

- `CA_PlayerCameras` 的 `ParameterDefinitions` 定义可调参数（臂长/偏移/FOV）；
- 组件 `CameraReference.Parameters`（`FCameraAssetReference::Parameters` FInstancedPropertyBag）按角色覆盖——BP_Okuma 组件上已存在参数覆盖结构，后续按角色调值即可，无需每个角色建独立 asset。

## 六、风险与坑（实施时逐条对照）

1. **重复激活语义**：`ActivateCameraForPlayerController` 对已激活组件是否刷新 view target，实现时验证（`EnsureCameraSystemHostIfNeeded` + `SetViewTarget(OwnerActor)` 路径）。
2. **旧组件抢视角**：隐藏旧成员后其 standalone 系统仍在跑；若实测旧组件抢回 view target，在隐藏旧成员处 `DeactivateCameraForPlayerController` 兜底。
3. **asset 级 EnterTransitions 未生效**：降级 rig 级配置（同参数）。
4. **角度连续性**：blend 两端旋转均为玩家 ControlRotation，理论上无感；若实测被拉向旧朝向，transition `InitialOrientation` 改 `PreviousYawPitch`。
5. **blend 时长单位**：秒（非帧）。
6. **切人节奏**：0.25s 过渡期内输入已切到新角色（视角短暂滞后可接受）；按住 Switch 连切由 `Started` 触发防连切（已有）。
7. **特写期间移动基准**：DoMove 读 ControlRotation Yaw，特写时镜头角度与玩家朝向可能分离——特写结束需同步（沿用 Sequence 对齐思路或直接以玩家 ControlRotation 为准，特写落地时定）。
8. **`Possess` 重置 ControlRotation**（2026-08-31 实测确认）：`APlayerController::Possess` 会把 ControlRotation 设为新 Pawn 的 ActorRotation → 切人后镜头被拉向角色移动朝向、玩家俯仰被清空。**修复已落地验证**：`SwitchToNextCharacter` 在 Possess 前保存 `GetControlRotation()`，Possess 后 `SetControlRotation()` 恢复。切人流程顺序：对齐位置+朝向 → 隐藏旧成员 → **保存 ControlRotation → Possess → 恢复 ControlRotation** → 激活新成员相机组件。另：`CR_ThirdPerson` 的 BoomArm 无输入槽时引擎内部自动用 `UDrivenControlRotationCameraNode`（镜头 = ControlRotation），机制本身正确，勿加冗余输入节点。

## 七、关键文件

- `ZZZ/Player/ZZZPlayerController`（切换流程 + 相机激活；Task #4 自动判定/TeamPanel 接线）
- `ZZZCombatRemake.Build.cs`（`GameplayCameras` 模块依赖）
- `/Game/ZZZ/Camera/`：`CA_PlayerCameras`（asset + EnterTransitions）、`CR_ThirdPerson`（主 rig）、`CDE_PlayerCamera`（director BP）、`CR_Closeup_*`（特写 rig，后续）
- 角色 BP：`/Game/ZZZ/character/BP_Okuma`、`BP_Jane`（GameplayCameraComponent 已挂，无需改动）
- 表现层：`ZZZ/Cues/GC_ZZZ_CameraShake`（不动）
