# TiDaoji

> **⚠️ NOT PG-SAFE (PR2)** — **生产 hide 仍是 SSDT + hooklib**。  
> InfinityHook 引擎已 **vendor 进 `TiDaoji/infinity_hook/`**（`IsStarted`/`Repair`/`Cleanup` 已修），**尚未接线** hide（PR3）。  
> **禁止**在「DSU restore 后长跑」工作流上当完成品。  
> 设计：`docs/2026-08-06-tidaoji-infinityhook-design.md`  
> 经典风险：SSDT 路径 → `0x109`；IH 层 B 长驻 CKCL/时钟指针也 **不称** PG-safe。

TiDaoji 由 [mrexodia/TitanHide](https://github.com/mrexodia/TitanHide) 分叉并 **全量改名**，用于在特定进程上隐藏调试器痕迹。  
通过 hook 多个 `Nt*`（当前仍为 SSDT）并改写返回值；向驱动写入 `HIDE_INFO`（PID + Type 位掩码）启用保护。

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

## DSU 工作流（设计目标；**PR1 尚未换引擎**）

画像 **A（用户已拍板）**：仅临时放开 **DSE** 装载，装完立即 restore；**PG 保持开启**。  
在 PR3（InfinityHook 接线）之前，**不要**在此工作流下长时间跑 PR1 二进制。

## Remarks

- 与 CR 等其它 InfinityHook 实例 **互斥**（PR3 起硬失败）。  
- **Never run on production; always VM.**  
- Upstream: original TitanHide project by mrexodia.
