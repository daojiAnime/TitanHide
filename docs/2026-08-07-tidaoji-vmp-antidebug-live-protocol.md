# TiDaoji live 反调试过检协议（VMP / 通用 Nt* 探针）

| 字段 | 值 |
|------|-----|
| **日期** | 2026-08-07 |
| **目标** | **反调试 live 过检**（非商业游戏 AC 隐身） |
| **工具** | `tools/antidebug_probe.exe`；驱动 L1；x64dbg + TiDaoji 插件 |
| **样本** | 自有 `123.dll` / 任意宿主；探针为 **自检进程**（与 VMP 同类 Nt* 面） |

> **通过标准**：调试附着下，hide 后关键探针由 DETECTED→CLEAN（或 detections 下降）。  
> **不是**：过 EAC/BE/Vanguard；不是 PG-safe。

---

## 1. 探针表

| ID | 探针 | 实现 | TiDaoji Type 位 |
|----|------|------|-----------------|
| P1 | PEB.BeingDebugged | 读 PEB | 用户态 hide 为主 |
| P2 | ProcessDebugFlags | NtQIP 0x1f | `HideProcessDebugFlags` |
| P3 | ProcessDebugPort | NtQIP 0x7 | `HideProcessDebugPort` |
| P4 | ProcessDebugObjectHandle | NtQIP 30 | `HideProcessDebugObjectHandle` |
| P5 | SystemKernelDebugger | NtQSI 0x23 | `HideSystemDebuggerInformation` |
| P6 | DebugObject Type 计数 | NtCreateDebugObject + NtQO | `HideDebugObject` |

输出文件默认：`C:\TiDaoji_antidebug_probe.txt`  
`detections=N`，进程 exit code = N。

---

## 2. 实验设计 A/B

| 轮次 | 条件 | 期望 |
|------|------|------|
| **A0** | 无调试器，直接跑 probe | 全 0 / detections=0 |
| **A1** | x64dbg 附着 probe，**TiDaojiUnhide**（关自动 hide） | P2–P4 至少部分 1；P1 常 1 |
| **B1** | 同会话 **TiDaoji** hide 后重跑 probe（或新开 debuggee） | P2–P4 应变 0；P1 依赖用户态 hide |

### 步骤（操作员 / 自动化）

```bat
:: build
cl /O2 /EHsc /Fe:tools\antidebug_probe.exe tools\antidebug_probe.cpp

:: A0
tools\antidebug_probe.exe -o C:\probe_A0.txt

:: A1/B1: x64dbg
init "....\antidebug_probe.exe"
TiDaojiUnhide
# run to exit -> C:\TiDaoji_antidebug_probe.txt 或 -o
TiDaoji
init 再次 / restart
# run -> 对比
```

脚本：`tools/run_antidebug_ab.ps1`（能自动化的部分）。

---

## 3. 通过 / 失败判定

| 结果 | 判定 |
|------|------|
| A0 全 0，A1 有检测，B1 关键内核探针 0 | **PASS** 内核 hide 有效 |
| A1=B1 仍检测 | **FAIL** 对应该 Type 位 / 未 hide 到 PID |
| A0 即有检测 | 环境有内核调试器 / 异常 |

记录模板：

| 探针 | A0 | A1 | B1 | 判定 |
|------|----|----|----|------|
| P1–P6 | | | | |

---

## 4. 与 123.dll / VMP

- `123.dll` 为 VMP 保护模块时，**壳内探针**可能多于上表。  
- 本协议先覆盖 **TitanHide 经典面**（与 TiDaoji Type 位对齐）。  
- 123.dll 专用：在 x64dbg 注入/加载后，对 **宿主 PID** hide，观察是否仍触发壳反调试（需人工看日志/是否退出）。  
- 路径示例（win-master）：`C:\Users\Administrator\Downloads\CS25\123.dll`

---

## 5. 结果归档

- **已填**：`docs/2026-08-07-tidaoji-vmp-antidebug-live-results.md`（2026-08-07 live）
- 更新研究矩阵「反调试 live」行

### 5.1 live 摘要（win-master 19045）

| 探针 | A0 | A1（驱动停） | B1（TiDaoji hide） |
|------|----|--------------|-------------------|
| P1 PEB | 0 | **1** | **0** |
| P2–P6 | 0 | 0 | 0 |
| detections | 0 | **1** | **0** |

- **判定**：PASS（A0 干净 / A1 有检出 / B1 清零）。差分主要在 **P1**。
- **P3/P4**：本环境 x64dbg 附着下 A1 亦为 0 → 不能单独证伪内核 hide。
- **B1 P1**：插件 `HidePid` 成功后 `DbgCmdExecDirect("hide")`（用户态）。
- **L1 重启**：`kdu -dse 0` → `sc start TiDaoji` → `kdu -dse 6`；KDU 成功常 **exit=1**，勿 `kdu && sc`。
- hold：`C:\TiDaoji_probe_hold` + go：`C:\TiDaoji_probe_go.txt`。

---

## 6. 修订

| Rev | 说明 |
|-----|------|
| 1 | 初版协议 + probe 工具 |
| 2 | 填入 win-master A0/A1/B1 live 摘要；KDU/hold 注意 |
