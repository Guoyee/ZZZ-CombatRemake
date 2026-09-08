# ZZZ MCP 资产操作坑清单

> **项目**: ZZZCombatRemake (UE 5.8) · 定稿 2026-09-08
> **用途**: 通过 ue-mcp 桥操作编辑器/资产时的已知坑与配方，供后续 session 直接照抄，勿重复试错。
> **权限**: 默认只读；**UI 资产**（UMG/WBP/UI 贴图）经用户当场允许后可写——边界见 `CLAUDE.md`「MCP 资产操作规则」。
> **来源**: 2026-09-08 HUD 会话（TeamPanel / 技能按钮 / 头像 / 圆角 / 灰度图标全流程）。

---

## 一、写入前的三条铁律

### 1. PIE 运行中 = 资产被锁

PIE 跑着的时候，下列调用会**失败但原因写得含糊**：

| 调用 | 表面报错 |
|---|---|
| `widget set_property` / `set_style` | `Failed to load WidgetBlueprint` / `WidgetBlueprint not found` |
| `asset save` | `success: false`（无 error 字段） |
| `asset set_texture_settings` | `Asset not found` |

**配方**：写资产前先 `editor(action="play_in_editor", pieAction="stop")`；写完再起 PIE 验证。

### 2. 类默认值（CDO）必须"编译 → 写 → 落盘"三步

`blueprint(action="set_class_default")` 写完**内存 CDO 有值、PIE 实例却是 None**。踩过两次：PC 的 `HUDWidgetClass`、角色 BP 的 `PortraitTexture`。

**配方**（顺序不能换）：
```
blueprint compile <BP>          # 先编译，生成类
blueprint set_class_default …   # 再写 CDO
editor save_dirty               # 必须落盘（dirtyCount 会显示 1）
```

**验证**：`editor(action="get_runtime_values", classFilter="<BP>_C", paths=["<Property>"])` 读 PIE 实例真值。

### 3. 嵌套控件模板 ≠ 类 CDO

HUD 里内嵌的 WBP（如 `WBP_ZZZHUD` 里的 `TeamPanel`）是**模板实例**，它的属性来自模板对象，改**类 CDO 无效**。

**配方**：改嵌套控件的属性一律用 `widget(action="set_property"/"set_style", assetPath=<父WBP>, widgetName=<子控件>)`。

---

## 二、控件树操作坑

| 坑 | 表现 | 配方 |
|---|---|---|
| **`bIsVariable` 未勾** | BP 图里 `K2Node_VariableGet` add 成功但 **pins 为空** | 先 `widget set_property … bIsVariable=true`，**编译**后再 add_node |
| **SizeBox 覆盖开关** | 只写 `WidthOverride=88` 不生效（仍铺满） | 同时写 `bOverride_WidthOverride=true` / `bOverride_HeightOverride=true` |
| **控件换父级** | 槽属性重置（`Size` 从 Fill 变回 Automatic，高度塌成几像素） | 移动后重设 `Slot.Size` / `Slot.*Alignment` |
| **Border 无内容时尺寸为 0** | 圆角矩形底盘撑不出 88×88 | 用 `Padding=(44,44,44,44)` 撑出期望尺寸（Border 无内容时靠 padding 定尺寸） |
| **Border 无贴图渲染白块** | `SetBrushFromTexture(null)` 后画白底 | 空值时 `SetVisibility(Hidden)`，别只清画刷 |
| **`set_root` 只能提升已存在控件** | 传新名字报 `Widget not found` | 建根用 `add_widget`（无 parent 即根） |

---

## 三、事件图（Blueprint 图）操作坑

### 1. exec 输出引脚**只能连一个目标**

给事件 exec 加新分支时会**静默顶掉旧连线**。本会话踩两次：
- 接血条数值 `SetText` 时顶掉了 `SetPercent` → 血条不动、文本在动；
- 接头像 `SetBrushFromTexture` 时顶掉了"当前槽放大"分支 → 两个槽一样大。

**配方**：用 `K2Node_ExecutionSequence` 并行两条链；或把节点**串联**（`A.then → B.execute`）。
**每次给事件 exec 加分支后，务必 `read_graph_summary` 确认原连线还在。**

### 2. `K2Node_FormatText` 的参数引脚类型由**第一次连接**决定

先接 int，之后再也接不了 FText（报 `Text is not compatible with Integer`）。
**配方**：接错就**删节点重建**，再按目标类型接。

### 3. 控件换类型后，图里的 `VariableGet` 仍握旧类型引用

Border → Image 之后，旧的 `Get Icon` 输出仍是 `Border Object Reference`，`connect_pins` 报类型不兼容。
**配方**：删掉旧的 Get 节点重新 add（按变量名重新解析）。

### 4. 其它

- `connect_pins` 的 `breakExistingSource` / `breakExistingTarget` 用于改接线（改前想清楚会断哪条）。
- `read_node_property` / `set_node_property` 的 `nodeName` 接受 add_node 返回的 hex id，也接受 `read_graph_summary` 里的短 id。
- 事件节点用 `override_function`（BP 可重写事件）创建，比手搓 `K2Node_Event` 稳。
- `K2Node_CallFunction` 的 `nodeParams` 形状：`{"functionName": "SetPercent", "className": "/Script/UMG.ProgressBar"}`。
- 函数名要先查引擎确认（`project(action="find_engine_symbol")`）：本会话 `Conv_FloatToInteger` 不存在，正确的是 `UKismetMathLibrary::Round(double) -> int32`。

---

## 四、贴图 / 画刷坑

### 1. 图标类 PNG 导入常被误判为 Normalmap

表现：`compressionSettings=Normalmap` + `pixelFormat=BC5`（只剩两通道）+ `sRGB=false` → 渲染发白/发灰。

**配方**（导入后立刻做）：
```
asset set_texture_settings
  {"compressionSettings": "Default", "sRGB": true, "lodGroup": "UI"}
```

### 2. 结构体画刷只能走 `set_style`（JSON，PascalCase）

- `widget set_property` 的 `Brush.ResourceObject` **不认**（`property not found`）；
- Border 的画刷属性叫 **`Background`**（不是 Brush）；
- 字段名用 PascalCase，`DrawAs` 用大写枚举名：

```json
{"ResourceObject": "/Game/ZZZ/UI/UI_Assets/SkillSpecial.SkillSpecial", "DrawAs": "IMAGE"}
{"DrawAs": "ROUNDEDBOX",
 "OutlineSettings": {"CornerRadii": {"X": 14, "Y": 14, "Z": 14, "W": 14},
                     "Width": 1, "Color": {"SpecifiedColor": {"R":1,"G":1,"B":1,"A":0.22}}}}
```

- **圆角矩形/圆形**：`DrawAs=ROUNDEDBOX` + `CornerRadii`（Slate 单位，被 DPI 缩放）；四角半径 = 边长一半 = 正圆。
- **描边**：`OutlineSettings.Width` + `Color`。

### 3. Border 没有 `SetBrushFromTexture`

那是 `UImage` 的方法。要在 BP 里运行时换贴图，控件**必须是 Image**。

### 4. 枚举写法两套，别记混

| 目标 | 写法 | 例子 |
|---|---|---|
| 控件**自身**属性 | 全名 | `HorizontalAlignment = "HAlign_Center"` |
| **槽（Slot）**属性 | 短名 | `Slot.HorizontalAlignment = "Center"` |
| 结构体里的枚举 | 大写枚举名 | `"DrawAs": "IMAGE"` |

---

## 五、只读诊断手段（最有用的一批）

| 目的 | 调用 |
|---|---|
| **看运行时控件树**（属性/贴图/百分比/文本） | `widget(action="get_runtime", className=…, maxDepth=3, includeLayout=true)` |
| 读 PIE 实例属性真值 | `editor(action="get_runtime_values", classFilter="BP_X_C", paths=["Prop"])` |
| 查自建日志（诊断根因） | `editor(action="search_log", query="CreateHUD")` |
| 看 BP 图结构与连线 | `blueprint(action="read_graph_summary")` |
| 查引擎符号名 | `project(action="find_engine_symbol", symbol=…)` |
| 看资产属性 | `widget(action="get_properties", widgetName=…)`（含槽块） |

### 截图注意

- `editor(action="capture_screenshot", target="pie")` 走 HighResShot → **只有 3D 场景，不含 UMG/Slate**。要截 UI 得用系统级截图（本会话用 PowerShell `System.Drawing.CopyFromScreen`）。
- standalone PIE 窗口的**渲染视口比窗口大约 1.4 倍** → 右下/右中锚点的控件落在窗口外看不见（左上锚点正常）。要目视验证时：切 **Play In Viewport**，或临时把控件锚到左上角截完再复位。

---

## 六、桥的能力边界（本机部署版为协议 v1，客户端 v1.3.7）

以下动作**不可用**（报 `Unknown method`，客户端还提示 "run npx ue-mcp update"）：
`get_object_properties` / `invoke_object_function` / `invoke_function` / `reflect_instance` /
`inspect_runtime_instances` / `list_pie_instances` / `get_blueprint_variable_default` /
`editor.capture_screen` / `project.list_available_plugins`。

需要这些能力时：改用等价的只读替代（见 §五），或 `editor(action="execute_python")`（有门禁，需给 `taskSummary` + `ruledOut`）。

**`asset read_properties` 对 Blueprint 路径会报 `Asset not found`** —— 读 BP 默认值改用 §一.2 的 `get_runtime_values`（PIE 内）或 `blueprint get_cdo_properties`（只对原生类有效）。

---

## 七、收尾清单（写 UI 资产时照走）

1. `play_in_editor stop`（解锁资产）
2. 改控件树 / 图（每步 `read_graph_summary` 或 `get_properties` 回读确认）
3. `blueprint compile`（0 error）
4. `editor save_dirty`（dirtyCount 归零）
5. `play_in_editor start` → `widget get_runtime` / `get_runtime_values` 验数据
6. 需要目视 → 系统截图（必要时临时挪锚点，截完复位）
7. 收尾：同步 `Docs/ZZZ-UI-Design.md` 状态行 + `Phase3-Plan` 顶部状态行 + 提交 git（规则 9）
