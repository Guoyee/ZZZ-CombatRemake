# Tools/AnimFix

5.2 → 5.8 动画资源迁移期间，手工排查并修复根骨骼轴向问题的一次性脚本留档。

## 背景

从旧工程导入的攻击动画（珂蕾妲 `K_Attack_01`）根骨骼位移轴向不对：FBX 里 Root 的位移记在 Z 上，
UE 需要的是 Y。表现为 root motion 抽取方向错误、角色位移不跟随朝向。

## 脚本

| 脚本 | 运行环境 | 用途 |
|---|---|---|
| `convert_fbx.py` | 普通 Python | **只读**。定位二进制 FBX 里 Root / Bip001 的 AnimationCurveNode 偏移与邻近 curve/key 数量，用来确认轴向记在哪条曲线上。不写文件。 |
| `fix_animation.py` | UE 编辑器内 Python | **方案 A**。Root：位移 `Y = -Z`、`Z = 0`、roll 固定 90°、scale = 1；Bip001：`Z = 0`。按文件内 `ANIM_LIST` 批量执行并保存。 |
| `rotate_root_bone.py` | UE 编辑器内 Python | **方案 B**（早期尝试）。保持 Bip001 原始旋转，位置按 X-90° 旋转、scale = 500，并把 `RootMotionRootLock` 设为 `AnimFirstFrame`。 |

## 运行

`fix_animation.py` / `rotate_root_bone.py` 依赖 `unreal` 模块，只能在编辑器里跑
（Output Log 的 Python 控制台，或 `Tools > Execute Python Script`）。
两者的资产路径硬编码在各自文件顶部（`ANIM_LIST` / `seq_path`），换资产先改路径。

`convert_fbx.py` 是普通 Python，路径从命令行传入：

```
python Tools/AnimFix/convert_fbx.py model/K_Attack_01.fbx
```

## 结论

Root 位移需 **Z→Y 并取反**（`Root.Y = -Root.Z`，`Root.Z = 0`），Bip001 的 Z 一并归零。

另：实测 `model/K_Attack_01.fbx` 里**没有** `Root\x00\x01T` / `Bip001\x00\x01T` 这类
AnimationCurveNode 模式（只有 `Root\x00\x01ModelS`）。二进制改 FBX 的路线在这个文件上没走通，
修复最终在编辑器内完成 —— 这也是这里同时留着编辑器侧脚本的原因。

## 长期方案

手工脚本已不作为常规流程。根骨骼变换清理改用插件 `Plugins/XXAnimRootEditor_full`
（[Awayee/XXAnimRootEditor](https://github.com/Awayee/XXAnimRootEditor) — *Remove transform of root
bone in animation sequences but maintain the final pose*），迁移到 5.8 的改动见 git 提交记录。

> 注：脚本引用的 `/Game/Asset/珂蕾妲/...` 与 `model/*.fbx` 目前仍在本地，可复跑；但两者都在
> `.gitignore` 范围内（`model/`、`Content/*` 除 `Content/ZZZ/`），**不入库**，换机器即失效。
> 路径硬编码，换资产先改脚本顶部。
