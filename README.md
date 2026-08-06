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

**路线（PR3 后）**：**PR4 插件/GUI → PR5 DSU runbook → 实机/VMP 矩阵最后**。矩阵不阻塞 PR4/5。

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
| x64dbg / x32dbg 插件 | `TiDaoji.dp64`（x64dbg）+ `TiDaoji.dp32`（x32dbg）；同一工程 `Release\|x64` / `Release\|Win32` |
| GUI | `TiDaojiGUI.exe`（Type/Driver/PID/SoftUnload，状态栏） |
| CE | `tools/ce/TiDaoji.lua` + `tools/tidaoji_cli.exe`（菜单/面板/`onOpenProcess`） |
| 装载 Runbook | `docs/2026-08-07-tidaoji-dsu-profile-a-runbook.md`（PR5，画像 A） |

> 用户口中的「管道名」在此实现为 **设备符号链接**（`\DosDevices\TiDaoji`），**不是** Windows Named Pipe。

### x64dbg / x32dbg 插件（PR4）

| 产物 | 调试器 | 放置目录（示例） |
|------|--------|------------------|
| `TiDaoji.dp64` | **x64dbg** | `x64dbg\x64\plugins\` |
| `TiDaoji.dp32` | **x32dbg** | `x64dbg\x32\plugins\` |

> 扩展名不同：`.dp64` ≠ `.dp32`。同一源码；内核驱动在 64 位 OS 上为 **x64 `TiDaoji.sys`**，x32dbg 调试 32 位进程时仍通过 `\\.\TiDaoji` 写同一驱动。

| 命令 / 菜单 | 作用 |
|-------------|------|
| 菜单 **Hide / Unhide / Status / Settings / Help** | 点选即执行 |
| `TiDaoji` | 对当前调试进程 `HidePid`（可重复下发） |
| `TiDaojiUnhide` | `UnhidePid` |
| `TiDaojiUnhideAll` | `UnhideAll` |
| `TiDaojiSoftUnload` | L2/L3 SoftUnload |
| `TiDaojiOptions [n]` | 读/写 Type 位掩码（`BridgeSetting` `TiDaoji/Options`，默认 **`0xFFF`**） |
| `TiDaojiName [svc]` | 设备名（默认 `TiDaoji` → `\\.\TiDaoji`） |
| `TiDaojiStatus` | 打开设备探针 + 会话状态 |
| `TiDaojiHelp` | 帮助 |

Settings UI：Type 勾选、Driver 名、Probe 设备、Apply&Hide、SoftUnload。  
自动：系统断点 → hide；结束调试 → unhide。

### Cheat Engine（原生插件 + 可选 Lua）

**支持原生 DLL 插件**（与 x64dbg 同类，CE Plugin SDK）：

| 产物 | 架构 | 部署 |
|------|------|------|
| `tools/ce/plugin/out/TiDaojiCE64.dll` | x64 CE | `CE\plugins\` → Edit→Plugins 启用 |
| `tools/ce/plugin/out/TiDaojiCE32.dll` | x86 CE | 同上 |

菜单：Hide / Unhide / Status / SoftUnload / Toggle AutoHide；ProcessWatch 可 AutoHide。  
构建：`tools\ce\plugin\build_ce_plugin.bat` + `deploy_ce_plugin.bat`。  
可选 Lua 面板：`tools/ce/TiDaoji.lua` + `tidaoji_cli.exe`。详见 **`tools/ce/README.md`**。

### OllyDbg / TitanEngine（PR4 深度打磨）

| 插件 | 产物 | 配置 ini（DLL 同目录） |
|------|------|------------------------|
| Olly1/2 | `TiDaojiOlly.dll` | `TiDaojiOlly.ini` → `[TiDaoji] DriverName=` / `Type=` |
| TitanEngine | `TiDaojiTE.dll` | `TiDaojiTE.ini` 同上 |

- 创建进程 → `HidePid`；退出/POSTDEBUG → `UnhidePid`；首次断点 → 用户态 PEB hide  
- 失败：`OutputDebugString` + **首次** MessageBox（带 Win32 码）  
- 共享逻辑：`TiDaoji/user_client.h`  
- 默认 `Type=0xFFF`（BIT1..BIT12，含 NtTerminateProcess）

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

**完整步骤与检查清单**：[`docs/2026-08-07-tidaoji-dsu-profile-a-runbook.md`](docs/2026-08-07-tidaoji-dsu-profile-a-runbook.md)（PR5）。

**画像 A（已拍板）**：仅临时放开 **DSE** 装载，装完立即 restore；**PG 保持开启**。  
`sc start` → InfinityHook `Start`（层 B 生效）→ **立即 restore DSE** → Hide。  
Restore **不**卸载驱动、**不**撤销层 B；hide 依赖仍存活的 IH 改写——有意取舍，不是“系统已干净”。

### 装载剖面（可选 L1 / L2 / L3）

| 剖面 | 一键 / 入口 | 说明 |
|------|-------------|------|
| **L1** DSE+sc | `tools\dse\load_tidaoji_profile_a.bat` | 默认研究路径；KDU 已入库 |
| **L2** manual map | `tools\loader\L2_kdmapper.bat` | 需自备 `KDMAPPER=` 外部 mapper |
| **L3** 多 provider | `tools\loader\L3_multi_provider.bat` | `TIDAOJI_MAPPER` / `TIDAOJI_PROVIDER` |
| **Auto** | `tools\loader\load_auto.bat` | 有 mapper 用 L2/L3，否则 L1 |
| 软卸载 | `tools\loader\soft_unload.bat` / `TiDaojiSoftUnload` | map 路径无 sc stop |

驱动：`DriverObject==NULL` → `IoCreateDriver`；`HIDE_INFO.SoftUnload`。  
详文：`docs/2026-08-07-tidaoji-loader-profiles-L1-L3.md`（含 **§12 对抗/红队检测矩阵**）  
DSE 工具：`tools/dse/README.md`

## Remarks

- 与 CR 等其它 InfinityHook 实例 **互斥**（`ConflictProbe` 硬失败）。  
- **Never run on production; always VM.**  
- **NOT PG-safe** — 消除经典 SSDT 0x109 路径 ≠ PatchGuard 形式化安全。  
- Upstream: original TitanHide project by mrexodia.
