# TiDaoji 装载剖面 L1 / L2 / L3

| 字段 | 值 |
|------|-----|
| **日期** | 2026-08-07 |
| **状态** | Optional multi-environment load paths |
| **驱动契约** | `DriverEntry` 支持 `DriverObject==NULL` → `IoCreateDriver`；`SoftUnload` 命令 |

> **NOT PG-safe.** 所有剖面只解决「如何把 TiDaoji 跑进内核」，不改变层 B 风险。  
> **不**在仓库内实现 BYOVD exploit 链；L2/L3 调用**你本机/外部** mapper。

---

## 剖面总表

| 剖面 | 手段 | 入口 | 卸载 | 适用环境 | 仓库脚本 |
|------|------|------|------|----------|----------|
| **L1** | 临时 DSE 窗口 + SCM | `tools/dse` + `sc start` | `sc stop` 或 SoftUnload | 研究机、有 admin、KDU 可用 | `tools/dse/load_tidaoji_profile_a.bat` |
| **L2** | Manual map（kdmapper 类） | 外部 `kdmapper.exe TiDaoji.sys` | **SoftUnload**（无可靠 sc stop） | 不能/不愿改 CI、要免服务痕迹 | `tools/loader/L2_kdmapper.bat` |
| **L3** | 多 provider / 换 BYOVD / 扩展 map | 外部 mapper + `TIDAOJI_MAPPER` | SoftUnload | iqvw 被拦、需换脆弱驱动 | `tools/loader/L3_multi_provider.bat` |
| **Auto** | 按环境探测选剖面 | — | — | 一键尝试 | `tools/loader/load_auto.bat` |

---

## 驱动侧兼容（已实现）

### L1（SCM）

- `DriverEntry(DriverObject, RegistryPath)` 正常路径。  
- `DriverUnload` → `FullTeardown`。

### L2/L3（manual map）

公开 kdmapper 行为（[TheCruZ/kdmapper](https://github.com/TheCruZ/kdmapper)）：

- Entry 时常 **`DriverObject == NULL`**。  
- TiDaoji：`DriverEntry` 检测 NULL → `IoCreateDriver(L"\\Driver\\TiDaoji", DriverInitialize)` 拿到真实 `DRIVER_OBJECT` 再建设备。  
- **`RegistryPath` 可空** → 设备名回落 `TiDaoji`。  
- **不要**在 Entry 里死循环；当前 IH `Start` 在 Entry 内完成并返回（DetectThread 异步）。

### SoftUnload

```text
HIDE_INFO { Command = SoftUnload, Type=0, Pid=0 }
WriteFile(\\.\TiDaoji, ...)
```

- 拆 IH + drain + 删设备/符号链接。  
- **L1 下** SCM 仍可能显示 RUNNING（僵尸服务）；应再 `sc stop/delete`。  
- **L2 下** 这是主要卸载方式；池映像本身可能仍驻留直到重启（map 限制）。

插件：`TiDaojiSoftUnload`  
冒烟：`tidaoji_smoke.exe --soft-unload`

---

## L2 操作要点

1. 准备 **外部** kdmapper（或兼容 fork），**不要**把脆弱驱动 CVE 利用强行当「官方依赖」提交策略以外的东西；本仓只留启动器。  
2. 设环境变量：

```bat
set KDMAPPER=C:\path\to\kdmapper.exe
set TIDAOJI_SYS=D:\src\TiDaoji.sys
tools\loader\L2_kdmapper.bat
```

3. 映射成功后应能：

```bat
tools\tidaoji_smoke.exe
```

4. 结束会话：

```bat
tools\tidaoji_smoke.exe TiDaoji 0 --soft-unload
```

5. **DriverEntry 必须快返回** —— 已满足；若 map 蓝屏查：双 IH、pattern、Entry 超时。

---

## L3 多环境支持矩阵（填空）

| 环境 | 推荐剖面 | 备注 |
|------|----------|------|
| Win10 19045 研究机 | L1（已验证） | KDU in `tools/dse` |
| Win10 19045 + iqvw 可用 | L2 | 外部 kdmapper |
| iqvw blocklist / AV | L3 换 provider | 自备 mapper/驱动，脚本只传参 |
| HVCI on | 往往全失败 | 需关 HVCI 或其它研究轨 |
| VBS / 安全启动 | L1/L2 均可能挂 | 见 g_CiOptions / VBS 文献 |
| 仅 testsigning | 正规签驱动 | 非本 L 系列 |

`tools/loader/providers.example.ini` 列出占位 provider 名，供 L3 脚本读取。

---

## 风险对照

| 风险 | L1 | L2/L3 |
|------|----|-------|
| BYOVD | KDU 临时 | mapper 内嵌脆弱驱动 |
| 服务痕迹 | 有 | 通常无 SCM 项 |
| 干净卸载 | sc stop | SoftUnload 部分 + 常需重启 |
| 与 PR3 契约 | 完整 | 需 SoftUnload 纪律 |
| 检测面 | 服务 + CI 篡改 | 池驱动 / PiDDB 清理痕迹 |

---

## 明确不做

- 仓库内完整 exploit PoC / 新 CVE 利用实现  
- 保证任意 Win11 24H2+ map 成功  
- 声称 map 后 PG-safe  

---

## 相关路径

| 路径 | 内容 |
|------|------|
| `TiDaoji/TiDaoji.cpp` | NULL Entry + SoftUnload |
| `tools/dse/` | L1 |
| `tools/loader/` | L2/L3/Auto 脚本 |
| `docs/2026-08-07-tidaoji-dsu-profile-a-runbook.md` | L1 细则 |
