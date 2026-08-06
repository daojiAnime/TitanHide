# TiDaoji × Cheat Engine

| 组件 | 作用 |
|------|------|
| `TiDaoji.lua` | CE autorun：菜单 + 面板 UI + `onOpenProcess` 自动 Hide |
| `../tidaoji_cli.exe` | 写 `\\.\TiDaoji` 的 CLI（Lua 调用） |
| `TiDaojiGUI.exe` | 独立 Win32 UI（不依赖 CE） |

> **需要** 已加载 `TiDaoji.sys`（L1 KDU/`sc` 等）。**NOT PG-safe。**

## 安装

```bat
:: 1) 编译 CLI（win-master 示例）
cd /d D:\src\TiDaoji\tools
cl /O2 /EHsc /I.. /Fe:tidaoji_cli.exe tidaoji_cli.cpp

:: 2) 拷到 CE autorun（路径按本机 CE 安装改）
copy /Y TiDaoji.lua            "D:\tools\CE76\autorun\"
copy /Y tidaoji_cli.exe        "D:\tools\CE76\autorun\"

:: 3) 重启 Cheat Engine（建议管理员，便于 CreateFile 设备）
```

启动后菜单栏应有 **TiDaoji**：

- Show panel… — 勾选 Type、Hide/Unhide/SoftUnload
- Hide / Unhide opened process
- Driver status

打开目标进程时，若勾选 **Auto Hide**，会走 `onOpenProcess` → `tidaoji_cli hide <pid>`。

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
