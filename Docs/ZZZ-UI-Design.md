# ZZZ UI 设计 —— HUD（TeamPanel / 技能按钮）& 敌人头顶条

> **项目**: ZZZCombatRemake (UE 5.8) · 定稿 2026-09-08（排程：HUD 步先行，见 Phase3-Plan §四）
> **状态**（2026-09-08）：**屏幕 HUD ✅ 已实施 + PIE 验证**（TeamPanel 槽序/头像/血条/能量/数值 + 技能按钮圆底盘/灰度就绪态；C++ + WBP 全部落地，见 §五）。**敌人头顶条未做**（排程下一项）。后续轮次事项见 §七 后置清单。资产经用户允许后可由 MCP 直接写（权限见 CLAUDE.md「MCP 资产操作规则」，坑见 `ZZZ-MCP-Pitfalls.md`）。
> **关联**：`ZZZ-Combat-System-Design.md`（Pawn-ASC、UI 事件驱动 §七.5、属性命名）；`ZZZ-Combat-Phase3-Plan.md` §四 排程；模板先例 `Variant_Combat` LifeBar（UWidgetComponent 用法参考，勿改 Variant）；`ZZZ/UI` 飘字系统（Screen-space WidgetComponent 先例）。

## 一、载体划分与总原则

| 载体 | 内容 | 交互 |
|---|---|---|
| 屏幕 HUD（Viewport UMG） | **左上 TeamPanel**（子状态栏 ×N）+ **右下技能按钮**（特殊技，E） | **纯键盘驱动**——控件纯状态指示、点击不响应（输入全走既有 EnhancedInput：空格=切人/支援判定、E=特殊技） |
| 敌人头顶条（世界空间） | 每敌人血条 + 失衡条 + 名字 | 无交互 |

1. **UI 是纯显示层，不写 ASC、不派发输入**——状态变化经事件流入（事件驱动，不轮询）。
2. **数据事件源**：`GetGameplayAttributeValueChangeDelegate`（Instant/Duration/Infinite GE 变化均触发）；绑定后初始手动拉一次值防首帧空白。死亡等 tag 态读属性派生（见 2.2/3.2）。
3. **属性名**（UZZZAttributeSet）：`Health/MaxHealth`、`Daze/MaxDaze`（敌）、`Energy/MaxEnergy`（队员，特殊技 Cost 50）。
4. 隐藏编队成员继续 Tick（Pawn-ASC 与 Possess 解耦）→ 属性事件照发，面板 HP/能量实时，无需轮询。
5. **轮转序真源 = PC `SquadClasses`**（"Squad roster in switch order"）；下一个切出 = `PickNextSquadClass`（roster 当前索引 +1 循环取模，**跳过 Eliminated**）。TeamPanel 槽位语义与切人逻辑共用同一数组，杜绝两份顺序。
   - ⚠ 实施修正（2026-09-08）：`SquadMembers` 按"首次登场/注册"序追加，**不可假设与 roster index 对齐**——成员实例一律按类查找（`GetSquadMemberByClass`，含世界扫描兜底覆盖尚未注册的初始角色）。

## 二、屏幕 HUD

### 2.1 布局
- **左上 TeamPanel**：横排子状态栏 ×N（N = `SquadClasses.Num()`，1–3）。栏布局统一：**左=头像占位，右=上血条下能量条**。槽 0 当前操作角色高亮描边；N<3 时超出的槽隐藏（含 N=1 只显 1 栏）。
- **右下技能按钮**：黑色圆形底盘 + 技能图标 + 键位角标 `E`（在圆下方）；就绪 = 彩色图标、未就绪 = 灰度图标（细则 2.3）。
- 根容器 `WBP_ZZZHUD`；创建/生命周期由 `UZZZPlayerController` 管（`CreateWidget`+`AddToViewport`，`TSubclassOf` 资产引用在 BP 填）。

### 2.2 TeamPanel（定案：左上 / 纯展示 / 轮转序）
- **槽位序**：设当前索引 = 当前操作角色在 `SquadClasses` 的下标，则槽 i = `SquadClasses[(当前索引 + i) % N]` 对应成员实例（`SquadMembers`）。槽 0 = 当前操作（高亮），槽 1 = 下一次切出（与 `PickNextSquadClass` 口径一致：存活者才可被选中切出）。
- **成员死亡（Eliminated）**：该槽**灰显占位**（HP=0、去饱和），不隐藏、不补位——切人自动跳过已由 `PickNextSquadClass` 保证，面板如实显示"此人已淘汰"；仅当队伍实有人数 N<3 时按人数隐藏空槽。
- **每槽数据**：绑定成员 ASC `Health`（血条 %）+ `Energy`（能量条 %，隐藏队员后台回能 1/s 照常反映）+ **HP 数值文本**（`BP_OnHealthUpdated` 带 Current/Max，BP 内 `Round → Conv_IntToText(bUseGrouping=false) → Format Text("{0}/{1}")` → 无千分位）。
- **头像**：成员 BP 的 `AZZZCharacter.PortraitTexture`（`EditDefaultsOnly`，每角色各自配），经 `BP_OnMemberBound(DisplayName, bIsCurrent, Portrait)` 推给 BP；未配则该槽头像隐藏（`IsValid` 分支）。
- **槽 0 放大**：`BP_OnMemberBound` 内 `SetRenderScale(1.0)`，其余槽 `0.72`（参考图"当前角色更大"观感）。
- **开局预加载**（2026-09-08 定稿）：PC `BeginPlay` 为 roster 里每个职业各生成一个隐藏实例（`SetActorHiddenInGame(true)` + 关碰撞，与退场后状态一致），初始角色直接登记不重复生成——保证**首次切换前每个槽都有真实成员**（否则未登场槽无实例，只能显示占位数值）。
- **刷新时机（Task #6 本体）**：override/接入 PC `OnPossess` → `RefreshSquad()`（替代现延迟一帧逻辑）：重算槽序（旋转）+ 高亮 + 重绑属性 delegate（防旧角色 delegate 残留）。招架支援/普通切换均 Possess 驱动 → 高亮与槽序自动一致，无额外信号。
- **数据访问**：PC 已有 `SquadClasses/SquadMembers`；若无只读访问器则加 `BlueprintPure` getter，**不改存储形态**。

### 2.3 技能按钮（定案：纯状态指示，仅特殊技，无大招槽）
- 绑定**操作角色**（Possessed pawn）ASC `Energy/MaxEnergy`；切换后随 `RefreshSquad` 重绑。
- 就绪阈值 = `Energy >= 50`（对齐 GA_Koleda_SpecialAttack `EnergyCost`；硬编码 50 并在注释标"与 GA 资产一致"，未来随 `UZZZCharacterData` 收敛为共享数据源，见 §七）。
- 表现（2026-09-08 定案）：**就绪 = 彩色图标（`SkillSpecial`）/ 未就绪 = 灰度图标（`SkillSpecialGray`）**——BP 内 `Branch(bReady) → SetBrushFromTexture(彩/灰)` 切换，不做 tint 压暗（压暗会看起来像半透明）；图标控件必须是 **Image**（`SetBrushFromTexture` 是 `UImage` 的方法，Border 没有）。**不做能量数值/环形指示**。不区分普通/强化分支（GA 内部按能量决策，按钮只表达"够不够放一次"）。
- 底盘：黑色正圆（Border `DrawAs=RoundedBox` + 四角半径 = 边长一半，`Width=0` 无描边）。
- 不做忙态（攻击中）遮罩——键盘输入缓冲已处理忙时按键，UI 忙态 = polish（§七）。

## 三、敌人头顶条

### 3.1 载体与创建
- `AZZZCombatEnemy` 构造器 `CreateDefaultSubobject<UWidgetComponent>(TEXT("HeadStatus"))`；**WidgetSpace = Screen**（自动面向相机，免旋转逻辑）；挂点 = 根上方常数偏移（初值 +190cm 左右，按骨架首测微调）。
- Widget 类 = `WBP_EnemyHead`（BP），Enemy C++ 持 `TSubclassOf`；**所有敌人生成即自带**。首版不做 MaxDrawDistance/距离裁剪。

### 3.2 内容与更新（定案：常显全部存活敌人）
- 内容：名字（上，小字；数据源 = 敌人 BP 可配 `FText`，人工填，可留空）+ 血条 + 失衡条（黄，区分血条）。
- 更新：BeginPlay 取自身 ASC → 绑 `Health/MaxHealth`（血条）+ `Daze/MaxDaze`（失衡条）delegate → 直接 `SetPercent`。
- **显隐**：常显；`Health==0`（死亡必经伤害，事件触发）→ 隐藏整个组件。若未来出现"非伤害归零"用例再改绑 `RegisterGameplayTagEvent(State.Dead)`。

## 四、类与资产清单

### C++（ZZZ 模块内新建，均 UUserWidget 派生）
| 类 | 职责 | 关键接点 |
|---|---|---|
| `UZZZPlayerHUDWidget` | 根容器（TeamPanel/技能钮引用）、初始化 | PC CreateWidget/AddToViewport |
| `UZZZTeamPanelWidget` | 槽数 = N、槽序旋转刷新 | `OnPossess → RefreshSquad`（Task #6 本体） |
| `UZZZTeamPanelEntryWidget` | 单栏：头像/血条/能量条/数值/高亮/灰显 | Health+Energy delegate；`BindMember` 推 DisplayName/bIsCurrent/Portrait |
| `UZZZSkillButtonWidget` | 就绪态（彩色/灰度图标切换）+ E 角标 | Energy delegate + Possess 重绑 |

### 角色侧 C++
- `AZZZCharacter.PortraitTexture`（`EditDefaultsOnly`，`UTexture2D*`）——小队栏头像数据源，每角色 BP 各自配。
- `AZZZPlayerController`：HUD 创建/刷新（`CreateHUD`/`RefreshHUD`）+ **开局预加载小队成员**（`BeginPlay`）+ 只读 roster 访问器（`GetSquadRosterSize`/`GetSquadRosterClass`/`GetSquadMemberByClass`/`GetCurrentSquadIndex`）。

### 敌人侧 C++（✅ 2026-09-08 已建，WBP 待做）
- `AZZZCombatEnemy`：`HeadStatus` WidgetComponent（Screen 空间、根上方 `HeadBarHeight`=190、DrawSize 240×84）+ `HeadWidgetClass`/`DisplayName`/`HeadBarHeight` 配置 + BeginPlay 创建 widget 并 `BindStatus`。

### 资产（`/Game/ZZZ/UI/`，✅ 2026-09-08 全部落地）
| 资产 | 内容 |
|---|---|
| `WBP_ZZZHUD` | Overlay 根 + TeamPanel（左上 48/32）+ SkillButton（右下 -72/-110） |
| `WBP_TeamPanel` | Border 底板（圆角 14 + 1px 白描边）+ `EntryContainer`（C++ BindWidget）+ `EntryWidgetClass` |
| `WBP_TeamPanelEntry` | 头像 Image + 血条/能量条 + HP 数值；4 个事件（MemberBound/Health/Energy/Eliminated）；当前槽 `SetRenderScale(1.0)` vs `0.72` |
| `WBP_ZZZSkillButton` | 黑色圆底盘 + 技能图标 Image + `E` 键位（圆角 6 小方块） |
| 贴图 | `UI_Assets/SkillSpecial`、`SkillSpecialGray`（灰度版）、`koleda`、`Miyabi`、`Nikole` |
| `WBP_EnemyHead` | 名字 Text + 血条 + 失衡条 —— **未做**（敌人头顶条步） |

## 五、实施分步（构建/验证纪律）

1. **C++ 逻辑件** ✅ 2026-09-08（4 个 UUserWidget 类 + `AZZZCharacter.PortraitTexture` + PC 集成；完整构建通过）。
2. **资产** ✅ 2026-09-08（WBP 全部落地，经用户授权由 MCP 直接写；配方与坑见 `ZZZ-MCP-Pitfalls.md`）。
3. **PIE 验证清单**（✅ 已验：①②③⑤；④⑥ 随敌人头顶条步）：
   ① 切换后槽序旋转、槽 0 放大跟随、槽 1 = 下次实际切出角色 —— ✅（Miyabi/koleda 头像互换实测）
   ② 隐藏队员血条/能量实时（后台回能）—— ✅（槽 1 能量 18% 在涨）
   ③ 能量 ≥ 50 → 图标转彩色；不足 → 灰度 —— ✅
   ④ 多敌人常显，受击血条掉/失衡涨，死亡隐藏 —— 未做（敌人头顶条步）
   ⑤ 队伍 1/2 人配置时多余槽不显示 —— ✅（roster 2 人 → 2 槽）
   ⑥ 10+ 次快速切换无 UI 悬挂/tag 残留 —— 待回归
4. **实测修正（2026-09-08）**：① 成员解析改按类查找（`SquadMembers` 非 index 对齐）；② 开局预加载全部成员（否则未登场槽无数据）；③ 就绪态从"tint 压暗"改"换贴图"（压暗观感像半透明）。

## 六、与后续轮次接点（本步少做防返工）

- **连携技**（失衡→演出）：头顶失衡条已是验收仪表；连携按键提示 UI 在连携步加（不入本步）。
- **大招**：Decibel 宿主落地后技能区加大招槽——技能按钮区做成**容器+可变子钮**结构，本轮仅挂特殊技钮，不加空槽。
- **特效**：GC 资产独立，与本设计无冲突。

## 七、后置清单（不阻塞本轮）

1. ~~头像纹理~~ ✅ 2026-09-08（每角色 BP 配 `PortraitTexture`）。遗留：头像是矩形硬贴，**圆形/圆角遮罩**待做（需材质或 9 宫格）。
2. 技能按钮忙态遮罩/连段可用提示（polish）。
3. 大招槽（Decibel 步，见 §六）。
4. 头顶条观感：受击淡出、MaxDrawDistance、失衡满闪（常显定案先跑，按观感后调）。
5. 点击交互 / 数字键 1/2/3 切人（定案纯展示，不做入本轮；若要 = SetInputMode + 新 IA，超出键盘驱动边界）。
6. 技能参数共享数据源（Cost 50 收敛，随 `UZZZCharacterData` 落地顺带）。
7. 技能按钮位置 = 右下（-72/-110）✅；精确手感可调。**观感遗留**：参考图的斜切造型需 9 宫格贴图（当前只有圆角矩形）。

## 八、待确认/风险

- 敌人头顶条挂点高度首测微调（±30cm）。
- PC 若已有 `OnPossess` 逻辑（招架支援链）→ 在既有函数内插入 `RefreshSquad()`，勿重复 override；HUD 创建时机取 PC 既有初始化锚点，避免与 `bIsSwitching` 首帧竞态。
- 死亡仅影响敌人侧已有用例；队员 Eliminated 路径若原型无触发入口，灰显逻辑仍按 HP=0 事件实现（防未来用例），PIE 可用临时手段验证。
- **PIE 窗口视口放大**（2026-09-08 实测）：standalone PIE 窗口的渲染视口比窗口大约 1.4 倍，右下/右中锚点的控件（技能按钮）落在窗口外看不见——不是布局错，切 **Play In Viewport** 或调整 PIE 窗口分辨率即可。目视验证时可用"临时改锚点 + 截图 + 复位"绕过。
