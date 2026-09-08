# 环境与构建 Setup（UE 5.8）

> CLAUDE.md 的构建入口指针；完整命令集中于此（2026-09-08 自 CLAUDE.md 迁出）。

## 常用命令

```bash
# 生成 VS 工程文件
"D:/Program Files/Epic Games/UE_5.8/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe" -ProjectFiles -Project="D:/UE5/ZZZCombatRemake/ZZZCombatRemake.uproject" -Game -Engine="D:/Program Files/Epic Games/UE_5.8"

# 构建编辑器目标（新增 UCLASS / UPROPERTY 后必须完整构建，UHT 重跑）
"D:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" ZZZCombatRemakeEditor Win64 Development -Project="D:/UE5/ZZZCombatRemake/ZZZCombatRemake.uproject"

# 打开项目
"D:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "D:/UE5/ZZZCombatRemake/ZZZCombatRemake.uproject"
```

## 迭代节奏

- 小改动可用 **Live Coding**（编辑器内）；新增 UCLASS / UPROPERTY 后必须完整构建（UHT 重跑）。
- 每个实施 Task 完成即构建 + PIE 手测，不跨 Task 堆积。
