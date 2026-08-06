# TiDaoji

> **⚠️ NOT PG-SAFE (PR3)** — 生产 hide 已接 **InfinityHook**（`k_hook`），**不再**走 SSDT 表写 / hooklib 内联补丁。  
> **仍不称 PG-safe**：层 B（CKCL SYSTEMCALL + GetCpuClock/Hvl/Halp）在驱动存活全程 long-lived。  
> 设计：`docs/2026-08-06-tidaoji-infinityhook-design.md`

TiDaoji 由 [mrexodia/TitanHide](https://github.com/mrexodia/TitanHide) 分叉并 **全量改名**，用于在特定进程上隐藏调试器痕迹。  
通过 InfinityHook 每 syscall 栈上指针交换拦截多个 `Nt*`，并改写返回值；向驱动写入 `HIDE_INFO`（PID + Type 位掩码）启用保护。

## 残余风险（诚实）

| 风险 | 状态 | 缓解 / 操作者动作 |
|------|------|-------------------|
| 层 B 长驻 → **仍 NOT PG-safe** | **接受（K20）** | 不声称 PG-safe；DSU 仅临时 DSE；支持矩阵实测 |
| 新 build 上 IH pattern 可能失效 | **硬失败装载** | `Initialize`/`Start` 失败则驱动不装；日志 `reason=InstallClocks` 等 |
| 与 CR 双 IH | **ConflictProbe 硬失败** | 先卸 CR；日志 `reason=ConflictProbe` |
| Unload 与 inflight syscall | **固定 drain** | 默认 **5s**（`TIDAOJI_UNLOAD_DRAIN_MS`；研究卸载可 10000）；无 per-syscall 引用计数 |
| hooklib / `SSDT::Hook` 仍在树里 | **写路径冻结** | 默认 stub；仅 `TIDAOJI_ALLOW_SSDT_FALLBACK` 编译真实写；生产未调用 |

族谱与新系统兼容对照（InfinityHookProMax / FiYHer / zhutingxf / CR）：见  
`docs/2026-08-06-infinityhook-lineage-newos-research.md`（含 Claude 审稿 §10.2）。

### 运维限制（研究机）

- **禁止与 CR 等同族 InfinityHook 双挂**；先 `sc stop` 卸 CR，再起 TiDaoji（否则 `ConflictProbe` 硬失败）。
- **避免 hook 存活期间长睡眠/休眠**：族谱物理机路径改 QPC/Halp 后，长睡唤醒可能时间漂移（zhutingxf 已知）。
- **不声称 24H2 已验证**；支持矩阵见研究文档 §8（live 格需实验室填）。
- 结构门禁：`tools/verify_research_landed.sh`（仓库根执行）。

## 身份面（PR1）

| 面 | 名称 |
|----|------|
| 驱动文件 | `TiDaoji.sys` |
| 服务名 | `TiDaoji`（= `\Device\TiDaoji` / `\\.\TiDaoji`） |
| 日志 | `C:\TiDaoji.log` |
| x64dbg 插件 | `TiDaoji.dp64`，命令 `TiDaoji` / `TiDaojiUnhide` / `TiDaojiOptions` / `TiDaojiName` |

> 用户口中的「管道名」在此实现为 **设备符号链接**（`\DosDevices\TiDaoji`），**不是** Windows Named Pipe。

## Features

- ProcessDebugFlags / ProcessDebugPort / ProcessDebugObjectHandle
- DebugObject (NtQueryObject)
- SystemKernelDebuggerInformation / SystemDebugControl
- NtClose 异常路径
- ThreadHideFromDebugger
- Protect DRx (NtGetContextThread / NtSetContextThread)
- SystemFirmwareVMScrub (`HideNtSystemVMInformation`, BIT 11) — 来自 lityrgia 选择性合并
- Hook 引擎：**InfinityHook**（PR3）；SSDT `GetFunctionAddress` 仅用于解析原件

## Compiling

1. Visual Studio 2022 + WDK  
2. 打开 `TiDaoji.sln` 编译  

## Installation（测试机 / 研究用途）

```bat
copy TiDaoji.sys %systemroot%\system32\drivers\
sc create TiDaoji binPath= %systemroot%\system32\drivers\TiDaoji.sys type= kernel
sc start TiDaoji
sc query TiDaoji
```

或：`install_driver.bat`（见仓库根目录）。

检查：DebugView 或 `C:\TiDaoji.log`。

### 服务名再变体（反字符串检测）

```bat
sc create NotTiDaoji binPath= %systemroot%\system32\drivers\TiDaoji.sys type= kernel
```

x64dbg：

```
TiDaojiName NotTiDaoji
```

## DSU 工作流（画像 A）

**画像 A（已拍板）**：仅临时放开 **DSE** 装载，装完立即 restore；**PG 保持开启**。  
`sc start` → InfinityHook `Start`（层 B 生效）→ Hide → **立即 restore DSE**。  
Restore **不**卸载驱动、**不**撤销层 B；hide 依赖仍存活的 IH 改写——有意取舍，不是“系统已干净”。

## Remarks

- 与 CR 等其它 InfinityHook 实例 **互斥**（`ConflictProbe` 硬失败）。  
- **Never run on production; always VM.**  
- **NOT PG-safe** — 消除经典 SSDT 0x109 路径 ≠ PatchGuard 形式化安全。  
- Upstream: original TitanHide project by mrexodia.
