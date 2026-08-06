# TiDaoji live 反调试过检结果（2026-08-07）

| 字段 | 值 |
|------|-----|
| **主机** | win-master（Windows 10.0.19045） |
| **驱动** | L1：`TiDaoji.sys` + KDU DSE 窗口（`D:\tools\kdu\kdu.exe`） |
| **探针** | `D:\src\TiDaoji\tools\antidebug_probe.exe` |
| **调试器** | x64dbg + TiDaoji 插件（MCP 2.3.0） |
| **协议** | `docs/2026-08-07-tidaoji-vmp-antidebug-live-protocol.md` |

> **范围**：TitanHide 经典 Nt* / PEB 面。**不是**商业游戏 AC；**不是** PG-safe。

---

## 1. 矩阵（自检 probe）

| 探针 | A0 无调试器 | A1 调试 + **无** TiDaoji | B1 调试 + TiDaoji hide | 判定 |
|------|-------------|--------------------------|------------------------|------|
| P1 PEB.BeingDebugged | 0 | **1** | **0** | B1 有效（用户态 `hide`） |
| P2 ProcessDebugFlags | 0 | 0 | 0 | 本环境 A1 无差分 |
| P3 ProcessDebugPort | 0 | 0 | 0 | 本环境 A1 无差分 |
| P4 ProcessDebugObjectHandle | 0 | 0 | 0 | 本环境 A1 无差分 |
| P5 SystemKernelDebugger | 0 | 0 | 0 | 干净 |
| P6 DebugObject Type | 0 | 0 | 0 | 干净 |
| **detections** | **0** | **1** | **0** | **PASS**（A0 干净；A1 有检出；B1 清零） |

### 原始落盘（win-master）

| 轮次 | 路径 | PID |
|------|------|-----|
| A0（无驱动） | `C:\TiDaoji_probe_A0.txt` | — |
| A0（驱动 ON） | `C:\TiDaoji_probe_A0_driver_on.txt` | 19272 |
| A1 自检 | `C:\TiDaoji_antidebug_probe.txt`（当时） | 2012 |
| B1 自检 | `C:\TiDaoji_probe_B1_self.txt` | 2828 |
| 远程 A1/B1 | `C:\TiDaoji_probe_A1_remote.txt` / `B1_remote.txt` | 18600（外部 NtQIP；本会话 port 仍为 0） |

### A1 摘录（驱动 **STOPPED**，x64dbg 附着）

```
pid=2012
PEB_BeingDebugged=1
ProcessDebugFlags=0
ProcessDebugPort=0
ProcessDebugObjectHandle=0
SystemKernelDebugger=0
NtQueryObject_DebugObjectType=0
detections=1
```

### B1 摘录（驱动 **RUNNING**，`TiDaoji` HidePid + 插件内 `DbgCmdExecDirect("hide")`）

```
pid=2828
PEB_BeingDebugged=0
ProcessDebugFlags=0
ProcessDebugPort=0
ProcessDebugObjectHandle=0
SystemKernelDebugger=0
NtQueryObject_DebugObjectType=0
detections=0
```

---

## 2. 操作时间线

1. `sc query TiDaoji` → STOPPED（DSE 恢复后 577 复现过）。
2. L1 重启：
   ```bat
   cd /d D:\tools\kdu
   kdu.exe -dse 0
   sc start TiDaoji
   kdu.exe -dse 6
   ```
   **注意**：KDU 成功时常 **exit code = 1**，不可 `kdu && sc start`。用 `call dse_off.bat`（脚本已把 0/1 当成功）或 `&` 分句。
3. hold：`echo hold> C:\TiDaoji_probe_hold`
4. x64dbg：`init "D:\src\TiDaoji\tools\antidebug_probe.exe"` → PID 2828（hold 循环）
5. `TiDaoji` → HidePid + x64dbg `hide`
6. `echo go> C:\TiDaoji_probe_go.txt` → 探针退出，写结果

---

## 3. 解释与局限

| 点 | 说明 |
|----|------|
| **B1 的 P1=0** | 插件 `cbTiDaoji` 在内核 Hide 成功后执行 `DbgCmdExecDirect("hide")`（x64dbg 用户态 PEB 清除），**不是**内核改 PEB。 |
| **A1 仅 P1=1** | 本机 x64dbg 附着下，自检 `ProcessDebugPort` / `ProcessDebugObjectHandle` 常为 0（与远程 probe 一致）。**不能**单靠 P3/P4 做 A1 差分；本矩阵差分主要靠 **P1**。 |
| **内核 hide 仍有价值** | 日志历史可见 `ProcessDebugPort by %d` 等 hook 命中；对 VMP 等主动 NtQIP 面仍是目标路径。本次 live 在 19045+x64dbg 上 **未**用 P3/P4 形成 A1 红灯。 |
| **hold 陷阱** | `C:\TiDaoji_probe_hold` 存在时裸跑 probe 也会等 go；A0 前必须 `del` hold。 |
| **DSE** | 卸载后 DSE 已 restore 时 `sc start` → **577**；必须再开 KDU 窗口。 |

---

## 4. 123.dll / VMP 样本

| 项 | 值 |
|----|-----|
| 路径 | `C:\Users\Administrator\Downloads\CS25\123.dll` |
| 同目录 | `123_dumped.dll`、`AntiDbgTest.exe`、`CS.exe` 等 |
| 本轮 | **未**注入 123 做壳内过检；仅经典探针矩阵 |

后续：对宿主 PID `TiDaoji` hide 后加载/注入 123，观察壳退出/反调试日志（人工）。

---

## 5. 判定

| 项 | 结果 |
|----|------|
| A0 全 0 | **PASS** |
| A1 有检出（P1） | **PASS**（基线有差分） |
| B1 detections=0 | **PASS** |
| 内核 P3/P4 live 差分 | **本环境未形成 A1 红灯** → 记 **inconclusive for P3/P4 under x64dbg** |
| 总评 | **经典面 live 过检：PASS（以 P1 为主差分）**；VMP 壳内 / 商业 AC **未测** |

---

## 6. 测后清理（2026-08-07 同日）

| 动作 | 结果 |
|------|------|
| `sc stop TiDaoji` | STOPPED（IH 卸） |
| `C:\TiDaoji_probe_*` / hold / go / log | 已删（矩阵已写入本文） |
| `tools\*.obj`、KDU `NalDrv*` 落盘 | 已删 |
| 僵尸 `TitanHide` SCM 项 | `sc delete`（sys 未在 drivers；防 dual-IH） |
| `D:\src\TiDaoji\._*` mac 垃圾 | 已清 |
| **保留** | `TiDaoji` 服务注册 + `drivers\TiDaoji.sys` + probe `.exe`（下次 L1 只需 DSE 窗口） |
| 脚本 | `tools/cleanup_live_residuals.bat` |

---

## 7. 修订

| Rev | 说明 |
|-----|------|
| 1 | 完成 A0 / A1 / B1 自检；记 KDU exit=1 与 P3/P4 局限 |
| 2 | 测后清理 + residual 脚本 |
