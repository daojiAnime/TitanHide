# TiDaoji × Cheat Engine

**可以。** CE 官方支持原生 DLL 插件（与 x64dbg 的 `.dp64` 同类），SDK 导出：

`CEPlugin_GetVersion` / `CEPlugin_InitializePlugin` / `CEPlugin_DisablePlugin`

| 组件 | 架构 | 作用 |
|------|------|------|
| **`plugin/TiDaojiCE64.dll`** | **x64 CE** | **原生插件**（主菜单 + ProcessWatch AutoHide） |
| **`plugin/TiDaojiCE32.dll`** | **x86 CE** | 同上，双架构 |
| `TiDaoji.lua` | 任意 | autorun 窗体 UI（可选，面板勾选 Type） |
| `../tidaoji_cli.exe` | x64 | Lua/脚本 CLI |
| `TiDaojiGUI.exe` | x64 | 独立 Win32 UI |

> **需要** 已加载 `TiDaoji.sys`。**NOT PG-safe。** 本机 CE 7.6 为 **64 位**，日常用 `TiDaojiCE64.dll`。

## 原生插件（推荐，对齐 x64dbg）

```bat
cd /d D:\src\TiDaoji\tools\ce\plugin
build_ce_plugin.bat
deploy_ce_plugin.bat
```

1. 重启 **Cheat Engine**（管理员更稳）
2. **Edit → Plugins**（或设置里插件列表）→ 添加  
   `D:\tools\CE76\plugins\TiDaojiCE64.dll` → Enable  
3. 主菜单出现：

| 菜单 | 快捷键 | 作用 |
|------|--------|------|
| TiDaoji: Hide opened process | Ctrl+Shift+H | HidePid（`OpenedProcessID`） |
| TiDaoji: Unhide opened process | Ctrl+Shift+U | UnhidePid |
| TiDaoji: Driver status | | 探测 `\\.\TiDaoji` |
| TiDaoji: SoftUnload | | L2/L3 卸载 |
| TiDaoji: Toggle AutoHide | | 进程创建时自动 hide |

配置 ini（DLL 旁）：`TiDaojiCE.ini` → `[TiDaoji] DriverName=` `Type=` `AutoHide=`

源码：`tools/ce/plugin/TiDaojiCE.cpp`（SDK：`plugin/sdk/cepluginsdk.h`）。

## Lua 面板（可选）

```bat
copy /Y TiDaoji.lua            "D:\tools\CE76\autorun\"
copy /Y tidaoji_cli.exe        "D:\tools\CE76\autorun\"
```

- Show panel… — 勾选 Type、Hide/Unhide/SoftUnload  
- 与原生插件可并存；Hide 都走同一驱动

## CLI 手测

```bat
tidaoji_cli status
tidaoji_cli hide 1234 --type 0xFFF
tidaoji_cli unhide 1234
tidaoji_cli unhide-all
tidaoji_cli soft-unload
```

## 与 x64dbg GUI 关系

| 宿主 | UI |
|------|-----|
| x64dbg | 插件菜单 + Settings 对话框 |
| 独立 | `TiDaojiGUI.exe` |
| CE | 本目录 Lua 面板 + CLI |

三者共用同一内核设备与 `HIDE_INFO` 协议；**不要**与 CR 双挂 InfinityHook。

## 排障

| 现象 | 处理 |
|------|------|
| Lua 报 CLI missing | 把 `tidaoji_cli.exe` 放进 `CE\autorun\` 或改 `findCli()` 路径 |
| OPEN FAIL | `sc query TiDaoji` → RUNNING；577 则再开 DSE 窗口 |
| Auto hide 无效果 | CE 未 attach；或 `autoHide` 未勾选；看 CE Lua 引擎输出 |
| 菜单不出现 | CE 版本 Menu API 差异 → 在 Lua 引擎执行 `showTiDaoji()` |
