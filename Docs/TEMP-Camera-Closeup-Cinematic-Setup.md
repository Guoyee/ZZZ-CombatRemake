# 【临时】招架特写 + 大招分镜镜头基建 — 操作指南

> **性质**: 临时工作便签（非定稿文档）· 2026-09-11 起 · PIE 验证通过后按规则 9 收尾（细节并入 `ZZZ-Camera-Architecture.md`，本文件可删）
> **状态**: C++ ✅ 已落地并编译通过（2026-09-11）· 资产 ⬜ 待人工操作 · PIE ⬜ 未验证
> **相关**: `Docs/ZZZ-Camera-Architecture.md`（相机架构定稿）· `Docs/ZZZ-Combat-System-Design.md` §4.9.2（招架）

---

## 0. 本次 C++ 改动（已编译进 DLL）

| 文件 | 改动 |
|---|---|
| `ZZZ/Player/ZZZPlayerController.h/.cpp` | `RequestCloseupCamera(UCameraRigAsset*)` / `ClearCloseupCamera()` / `GetRequestedCloseupRig()`；`PlayCinematic(ULevelSequence*, AActor*, FName)` / `StopCinematic()` / `IsCinematicPlaying()`；控制台命令 `ZZZ.PlayCinematic` |
| `ZZZ/Abilities/ZZZAssistDefensive.h/.cpp` | `CloseupRig` / `CloseupDuration` 配置槽；`StartCloseupCamera()`（定格帧切入）/ `StopCloseupCamera()`（定时归还 + EndAbility 兜底） |
| `ZZZCombatRemake.Build.cs` | +`LevelSequence` / `MovieScene` |

**机制一句话**：C++ 只写"请求"到 PC；**真正切镜头的是 director（`CDE_PlayerCamera`）**——它每帧读请求决定 push 哪个 rig（引擎固有行为：director 每帧调 `ActivateCameraRig`，绕过它的镜头会被下一帧顶掉）。

---

## 1. 招架特写（本次重点）

### 1.1 新建 `CR_Closeup_Parry`（⚠ 必须锁定角度，否则跟鼠标转）

1. 内容浏览器 → `/Game/ZZZ/Camera/` → 右键 `CR_ThirdPerson` → **Duplicate** → 改名 `CR_Closeup_Parry`
2. 双击打开 → **删掉 `BoomArmCameraNode_0`**，用下面两个节点替换（原始 BoomArm 在 `InputSlot` 为空时自动吃玩家 ControlRotation——旋转和位置都跟鼠标，特写不能用）：
   - 加 **`SetLocationCameraNode`**（固定机位）：
     - `Location` = **`(-220, 250, 80)`**（角色右后方近机位 = 过肩侧拍，主角近景 + 前方敌人同框）
     - `OffsetSpace` = **`Pawn`** ← 关键，改掉默认的 OwningContext
   - 加 **`SetRotationCameraNode`**（固定朝向）：
     - `Rotation` = **`(0, 0, 0)`**（= 角色朝向 → 从侧后方看向角色前方；想换角度改 Yaw，如 -25 更偏正面）
     - `OffsetSpace` = **`Pawn`** ← 关键，改掉默认的 CameraPose
3. 节点链接成：`Set Location → Set Rotation → FieldOfViewCameraNode`（原 BoomArm 的位置）
4. 选中 **FieldOfViewCameraNode_0** → `FieldOfView`：`80` → **`38`**（长焦压缩感）
5. 保存

> **机位微调**（不用重编译，PIE 直接看）：`SetLocation` 的 X 越小越近、Y 越大越偏侧面、Z 是高度；`SetRotation` 的 Yaw 调拍摄角度。
> **CollisionPush 节点**：建议删掉（或 `bIsEnabled` = false）—— 特写机位离角色仅 2-3m，贴墙时避障推镜头会让构图乱跳。
> **省事替代**：嫌删 BoomArm 重连麻烦，保留它、在它**后面**接 SetLocation → SetRotation 也行（结果一样，BoomArm 的输出被完全覆盖，只是多一次无用求值）。

### 1.1b 切入/切出过渡速度

**查找规则**（`TransientBlendStackCameraNode.cpp:584-660`，按序取第一个条件匹配的）：

```
① 旧 rig 的 Exit Transitions
② 旧 CameraAsset 的 Exit Transitions
③ 新 rig 的 Enter Transitions
④ 新 CameraAsset 的 Enter Transitions   ← 默认落这里（CA_PlayerCameras 的 0.25s）
```

**默认行为**：切入和切出都是 **0.25s**（都 fallback 到 `CA_PlayerCameras` 的 EnterTransitions）。

**分别调整**：

| 想要控制 | 配在 | 命中位置 |
|---|---|---|
| **切入**特写的速度 | `CR_Closeup_Parry` 的 **Enter Transitions** | ③（新 rig = 特写） |
| **切出**特写的速度 | `CR_Closeup_Parry` 的 **Exit Transitions** | ①（旧 rig = 特写，优先级最高） |

配置方式（与 `CA_PlayerCameras` 上那条相同）：rig 细节面板 → `Enter Transitions` / `Exit Transitions` → **+** → 展开 `Blend` → 选 `Easing Blend Camera Node` → 设 `Blend Time`（秒）。硬切 = 换 `Pop Blend Camera Node`。

> ⚠ **rig 的过渡不在细节面板**（`EnterTransitions`/`ExitTransitions` 是 `UPROPERTY(Instanced)` 无 `EditAnywhere`，`CameraRigAsset.h:82-84`），也**不在节点图**里。入口 = **Camera Rig 编辑器工具栏的 `Transitions` 按钮**（与 `Node Hierarchy` 并列的一对 Toggle 按钮，`CameraRigAssetEditorCommands.cpp:32-35`）→ 画布切到过渡图（水印 TRANSITIONS）→ 在其中编辑 Enter（进场）/ Exit（出场）两组过渡。
> 对照：`CA_PlayerCameras` 上的那条是 **CameraAsset 编辑器**的 "Shared Transitions"（资产级，一处管所有 rig）。

推荐起点：**切入 0.06s**（快切、打击感）/ **切出 0.25s**（与切人过渡一致）。

**Transition 的结构**（`CameraRigTransition.h:150-178`）：

| 字段 | 作用 |
|---|---|
| `Conditions[]` | 条件列表，**全部通过**才用这条过渡；**留空 = 无条件通过** |
| `Blend` | 混合方式（Easing / Pop / Linear / …） |
| `bFreezePreviousCameraRigs` | 冻结下层 rig（本项目不用） |
| `InitialOrientation` | 新 rig 初始朝向策略（本项目不用） |

**可用条件（5.8 只有两种）**：
1. **`Is Camera Rig`**：`Previous Camera Rig` / `Next Camera Rig` —— 留空 = 不限制；填了则必须等于对应 rig（旧/新）
2. **`Gameplay Tag`**：对旧/新 rig（或 camera asset）的 `GameplayTags` 做 GameplayTagQuery（可给 rig 打 tag 再匹配）

**顺序敏感**：同一列表内按序取**第一条**全部条件通过的 → 带条件的必须排在无条件的前面。

**两种配法**：

| | 方案 A（推荐） | 方案 B |
|---|---|---|
| 配在哪 | `CR_Closeup_Parry` 自己的 Enter/Exit Transitions | `CA_PlayerCameras` 上，用 `Is Camera Rig` 条件筛 |
| 条件 | 不用配（只有进出特写才用它） | `Next=CR_Closeup_Parry` → 0.06s；`Previous=CR_Closeup_Parry` → 0.25s |
| 命中位置 | Enter 在 **③**、Exit 在 **①**（优先级高） | 资产级，**④**（①②③ 都空才轮到） |

方案 A 内聚、优先级高；方案 B 可在一个面板看全所有过渡，但排在查找链末端。

### 1.1c 谁配什么（三个资产的最终分工）

⚠ **关键**："切入特写"和"切人"的 `From rig` **都是 `CR_ThirdPerson`**（切人 = 角色 A 主 rig → 角色 B 主 rig，rig 资产同一个）——所以 `CR_ThirdPerson` 上配无条件过渡会**同时误伤两种切换**（需求速度还不同）。

| 资产 | 配什么 | 为什么 |
|---|---|---|
| `CA_PlayerCameras` | **保留现有 0.25s 那条，不动** | ① 切人过渡（本职）② 所有未配置情况的兜底 |
| `CR_ThirdPerson` | **不配（保持空）** | 配了会命中"切人"与"切入特写"（From 都是它），无法区分 |
| `CR_Closeup_Parry` | **Enter** = Easing **0.06s**（必配，否则切入也是 0.25s）<br>**Exit** = Easing 0.25s（可选，不配则 fallback 到 CA 的 0.25s，效果相同） | 控制进出特写速度 |

**各场景最终生效的过渡**：

```
切人 A→B    → ④ CA 的 0.25s
切入特写    → ③ 特写 rig 的 Enter  0.06s   （快切）
切出特写    → ① 特写 rig 的 Exit   0.25s   （若配；否则 ④ CA 的 0.25s）
首次激活    → 栈为空，直接 100%（不查过渡）
```

### 1.2 改 `CDE_PlayerCamera`（director BP）

打开 `/Game/ZZZ/Camera/CDE_PlayerCamera`（EventGraph 现在只有 2 个节点）。

**先确认**：现有 `Activate Camera Rig` 节点的 `CameraRig` 参数 = `CR_ThirdPerson`（它以后是"无特写"分支）。

**① 断开旧连线**：`Event RunCameraDirector.then` → 现有 `ActivateCamera Rig.execute`（节点保留）。

**② 添加 5 个新节点**（图空白处右键搜索）：

| # | 搜索 | 设置 |
|---|---|---|
| 1 | `Find Evaluation Context Owner Actor` | actor class 下拉选 **Pawn** |
| 2 | `Get Controller` | Target is Pawn |
| 3 | `Cast To ZZZPlayerController` | |
| 4 | `Get Requested Closeup Rig` | Target is ZZZ Player Controller |
| 5 | `Is Valid` | 选 Object 版本 |

再**复制**一份现有 `Activate Camera Rig` 节点（Ctrl+C / Ctrl+V）作特写分支。

**③ 数据线（白线）**：
```
[1] Find Evaluation Context Owner Actor
   └─ Return Value ─→ [2] Get Controller
                        └─ Return Value ─→ [3] Cast To ZZZPlayerController
                                             └─ As ZZZ Player Controller ─→ [4] Get Requested Closeup Rig
                                                                               └─ Return Value ─→ [5] Is Valid
```

**④ 执行线（红线）**：
```
Event RunCameraDirector ─then─→ [5] Is Valid
                                  ├─ True  ─→ 新 Activate Camera Rig（CameraRig pin ← [4] Return Value）
                                  └─ False ─→ 原 Activate Camera Rig（保持 CR_ThirdPerson）
```

**⑤ Compile + Save**。

> **降级安全**：Cast 失败 → `Get Requested Closeup Rig` 返回 null → Is Valid=False → 主 rig，不会黑屏。
> `Find Evaluation Context Owner Actor` 带 `DeterminesOutputType`，传 Pawn 时返回类型自动是 Pawn，无需再 Cast。

### 1.3 配 `GA_Koleda_AssistDefence`

打开 `/Game/ZZZ/Abilities/Koleda/GA_Koleda_AssistDefence` → 细节面板搜 `Closeup`：

| 属性 | 值 |
|---|---|
| `Closeup Rig` | `CR_Closeup_Parry` |
| `Closeup Duration` | `1.2`（秒；**从按下空格瞬间起算**，需覆盖 最坏入场余量 ≈0.66s + 定格 0.3s + 收势一点；0 = 持续到 GA 结束） |

保存。

### 1.4 PIE 验证

1. PIE 开始，走到敌人面前等**黄闪**（敌人攻击预警）
2. 黄闪瞬间按**空格**（招架）
3. 预期：**按下空格瞬间切特写**（与招架者入场同拍）→ 覆盖 冲入→摆架势→定格 → 约 1.2s 后归还主视角
4. Output Log 核对：
   - `UZZZAssistDefensive: closeup camera requested ('CR_Closeup_Parry', 1.20s).`
   - `UZZZAssistDefensive: closeup camera released — returning to the main rig.`

**排障速查**：

| 症状 | 检查 |
|---|---|
| 完全没切镜头 | ① director BP 是否编译保存 ② GA 的 Closeup Rig 是否填了 ③ 日志有没有 `closeup camera requested` |
| 切了一下立刻回 | 特写 rig 的 ExitTransitions/主 rig 的 EnterTransitions 被误配；或 GA 提前结束（看 `released` 日志时机） |
| 切不回来 | director BP 的 False 分支没接 / CameraRig 不是 CR_ThirdPerson |
| 构图不对 | 只改 `CR_Closeup_Parry` 的 `SetLocation` / `SetRotation` / FOV，不用重编译 |
| 镜头仍跟鼠标转 | BoomArm 没删干净（见 §1.1），或 `SetLocation`/`SetRotation` 的 `OffsetSpace` 没改成 `Pawn` |
| 切入/切出速度不对 | 见 §1.1b：分别配特写 rig 的 Enter / Exit Transitions |

### 1.5 调试技巧 · 不开招架单独调机位

director BP 改完后（1.3 可跳过），PIE 里让 Claude 通过 MCP 直接调：
- `RequestCloseupCamera(CR_Closeup_Parry)` — 立刻切特写看构图
- `ClearCloseupCamera()` — 归还

改 `SetLocation` / `SetRotation` → 保存 rig → 再点一次，反复调到满意。

---

## 2. 大招分镜基建测试

> 大招 GA 尚未落地（Phase 5，Decibel 宿主持定）。本组只验证**镜头系统本身**：接管 + 归还。
> 正式形态：**pose 阶段**（无伤害）Sequence 全包（相机 + 角色动画 + 背景遮挡），**释放阶段**归还主 rig 走 GA 蒙太奇。

### 2.1 建测试 Sequence

1. `/Game/ZZZ/Camera/` → 右键 → **Cinematics → Level Sequence** → 命名 `LS_Test_Cinematic`
2. 打开 → 点工具栏 **+ Camera** 创建 CineCameraActor（自动绑定）
3. 添加 **Camera Cut Track**（点 "+ Track" → Camera Cut），把相机绑上
4. 给相机做一段运镜（拖时间轴，给相机 Transform 打 2-3 个关键帧，时长 ~3 秒）
5. 保存

### 2.2 ⚠ 关键设置：归还

**Camera Cut 轨道里每个 section** → 右键 → **When Finished** 设为 **Restore State**。

> 这是"播完自动回到角色主相机"的唯一开关。5.8 的 `FMovieSceneSequencePlaybackSettings` **没有** `bRestoreState` 字段（旧资料常见误导），它是 per-section 设置。

### 2.3 控制台测试

PIE 中按 `~` 打开控制台：

```
ZZZ.PlayCinematic /Game/ZZZ/Camera/LS_Test_Cinematic
```

预期：相机切到 Sequence 相机播运镜 → 播完自动归还角色视角。

- 重复输入 = 先停上一个再播（同一时间只允许一个过场）
- 蓝图侧接口：`PlayCinematic(Sequence, BindActor, BindingTag)` / `StopCinematic()` / `IsCinematicPlaying()`
- 动态绑定（大招正式用）：Sequence 里给角色 Object Binding 打 **Tag**（Sequencer 大纲中选中 binding → Details/Tag 区域），调用时传 `BindingTag`，运行时把当前角色绑过去 → 大招在任意位置释放都能对位

---

## 3. 机制速查（排障用）

```
① 谁想用   UZZZAssistDefensive::StartCloseupCamera()      ← 按下空格瞬间（GA 激活，C++）
            └→ PC->RequestCloseupCamera(rig)               ← 只写请求，不切镜头
② 谁执行   CDE_PlayerCamera::RunCameraDirector             ← ★ rig 唯一激活点（每帧）
            └→ ActivateCameraRig(请求的 rig 或 CR_ThirdPerson)
③ 谁归还   请求清空（0.8s 到期 / GA 结束）→ director 下一帧 push 主 rig
            → 特写 rig 被 blend out + pop（用主 rig 的 EnterTransitions 过渡）
```

**为什么必须走 director**：director 每帧调 `ActivateCameraRig`；引擎 `TransientBlendStackCameraNode::Push`（`Plugins/Cameras/GameplayCameras/.../Core/TransientBlendStackCameraNode.cpp:28-78`）的去重只对"栈顶同 context 同 rig"生效 —— 任何绕过 director 的镜头都会被下一帧顶掉。

**为什么不用组件侧 `ActivatePersistentVisualCameraRig`**：manager 模式（`bRunStandaloneCameraSystem=false`）下组件不建 camera system → `HasCameraSystem()==false` → 组件侧 Activate/Deactivate 系列全部走错误分支（`GameplayCameraComponentBase.cpp:142-149`）。

**大招收管路径**：CameraCut → `PC->SetViewTarget(CineCameraActor)` → manager 建 `FActorCameraEvaluationContext` 压栈（对 CineCameraActor 是正确行为）；归还 → `SetViewTarget(角色 Pawn)` → 命中"栈内既有 context 移顶"分支（`GameplayCamerasPlayerCameraManager.cpp:320-346`），不踩 actor-copy 陷阱。

---

## 4. 待验证 / 风险点

- [ ] 招架特写切入瞬间与"切人 blend"叠加的观感（按键早时切人 0.25s 过渡未完）
- [ ] `SetLocation` / `SetRotation` 机位与角度实拍微调
- [ ] 切入/切出过渡速度（§1.1b）配置后的观感
- [ ] CloseupDuration=1.2s（按键起算）与 入场(≤0.66s)+定格(0.3s)+收势 的节奏是否合适
- [ ] 大招 Sequence 的 CameraCut 归还实测（源码推论为可行，未跑）
- [ ] 大招角色绑定 tag 的确切 UI 位置（Sequencer 大纲 binding 的 Tag 设置入口）

## 5. 验证通过后的收尾（规则 9）

1. `ZZZ-Camera-Architecture.md`：§四"特写预留"更新为已落地（含"rig 不会自动归还、须 director 驱动"的机制更正）
2. `ZZZ-Combat-System-Design.md` §4.9.2：补招架特写一节
3. `Docs/ZZZ-Combat-Phase3-Plan.md` 顶部状态行更新
4. 删除本临时文件
