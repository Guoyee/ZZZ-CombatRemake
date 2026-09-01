# 四段普攻连段 — 实现记录（Phase 2，已完成）

> **状态**: 已实现并 PIE 验证。设计决策已合入 `ZZZ-Combat-System-Design.md` §4.3/§4.7，本文仅保留落地要点。
> **旧版备份**: `Docs/archive/ZZZ-Basic-Attack-Combo-Design.md.orig`（勿读，仅查证历史）

- 每段攻击 = 一个 Montage（Attack + Recovery 双 Section）+ 一个 GA（GA_BasicAttack_01~04，BP 资产）；连段 = Ability 切换，非 Section 跳转。
- 死区：`AnimNotify_SendGameplayEvent(Event.AnimNotify.BeginInputBuffer)` 放在伤害帧后 2-3 帧；各段死区长度 = 拖动 Notify 位置，纯动画侧调整，不涉及代码/数据资产。
- Recovery 段嵌 `AnimNotifyState_AbilityWindow`（CanCombo）；Root Motion 全程开；BlendIn 0.05-0.1s。
- `WaitInputBuffer` 状态机：死区→监听→写入 PC 缓冲；`WaitCombo` 事件驱动消费（窗口+缓冲双条件）。
- 连段过渡**先激活下一段再 EndAbility**；末段 NextComboAbility=null。
- 调试：`showdebug abilitysystem` 观察 CanCombo / Buffered tag 增删。
