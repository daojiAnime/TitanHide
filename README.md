# TiDaoji

> **⚠️ NOT PG-SAFE (PR3)** — 生产 hide 已接 **InfinityHook**（`k_hook`），**不再**走 SSDT 表写 / hooklib 内联补丁。  
> **仍不称 PG-safe**：层 B（CKCL SYSTEMCALL + GetCpuClock/Hvl/Halp）在驱动存活全程 long-lived。  
> 设计：`docs/2026-08-06-tidaoji-infinityhook-design.md`  
> 残余风险：IH 栈/pattern 失效、与其它 InfinityHook（如 CR）冲突、Unload 竞态、未来 PG 规则变化。

TiDaoji 由 [mrexodia/TitanHide](https://github.com/mrexodia/TitanHide) 分叉并 **全量改名**，用于在特定进程上隐藏调试器痕迹。  
通过 InfinityHook 每 syscall 栈上指针交换拦截多个 `Nt*`，并改写返回值；向驱动写入 `HIDE_INFO`（PID + Type 位掩码）启用保护。

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
- Upstream: original TitanHide project by mrexodia.
