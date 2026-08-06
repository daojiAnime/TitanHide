# TiDaoji 装载 Runbook — DSU 画像 A（仅临时 DSE）

| 字段 | 值 |
|------|-----|
| **PR** | PR5 |
| **日期** | 2026-08-07 |
| **画像** | **A only**（K20）：临时放开 **DSE** 以便装载未签名 `TiDaoji.sys`；**PG 全程开启** |
| **关联** | 设计 `docs/2026-08-06-tidaoji-infinityhook-design.md`；研究 `docs/2026-08-06-infinityhook-lineage-newos-research.md` |
| **驱动设备** | `\\.\TiDaoji`（服务名默认 `TiDaoji`；可改，见下） |

> **NOT PG-safe。** InfinityHook 层 B（CKCL / GetCpuClock / Hvl / Halp）在 `sc start` 成功后 **long-lived** 直至卸载驱动。  
> DSU **restore 只恢复 DSE/签名策略**，**不**撤销 hook、**不**卸载驱动。

---

## 0. 红线（先读）

1. **禁止与 CR / 其它 InfinityHook 双挂**。先卸 CR：
   ```bat
   sc stop <CR服务名>
   sc query <CR服务名>
   ```
   TiDaoji `Start` 会 `ConflictProbe` 硬失败（日志 `reason=ConflictProbe`）。
2. **禁止永久关 PatchGuard**（画像 B 不在范围内）。
3. **hook 存活期间避免长睡眠/休眠**（族谱 QPC/Halp 路径已知时间漂移）。
4. **仅研究 / VM**；非生产。
5. 实机多 build 矩阵 **不** 作为本 runbook 门禁（最后阶段再填研究 §8）。

---

## 1. 工具占位（本机填写）

| 角色 | 本机候选 / 填空 | 用途 |
|------|-----------------|------|
| DSE 窗口工具 | **win-master 已验证**：`D:\tools\kdu\kdu.exe`（亦可用 DisabledDSE/DHS） | 短暂允许未签名驱动装载 |
| 进入 DSE 窗口 | `kdu.exe -dse 0` | 装载前 |
| restore DSE | `kdu.exe -dse 6`（该机 g_CiOptions 常态为 6） | **start 成功后立即** |
| 验证 DSE 已恢复 | `kdu -dse` 读回 / 再装未签名驱动应 577 | **只证 DSE，不证 hook 已卸** |
| 内核日志 | DebugView 或 `C:\TiDaoji.log` | `[TIDAOJI]` / `[TIDAOJI][IH]` |
| 用户态 | x64dbg + `TiDaoji.dp64` 或 `TiDaojiGUI.exe` | 写 `HIDE_INFO` |

> 具体二进制路径因环境而异；**画像不因工具而改**。

---

## 2. 产物

| 文件 | 说明 |
|------|------|
| `TiDaoji.sys` | Release\|x64 内核驱动（InfinityHook 生产 hide） |
| `TiDaoji.dp64` | x64dbg 插件（PR4：`TiDaojiHelp` / `Status` / …） |
| `TiDaojiGUI.exe` | 可选 GUI |
| `TiDaojiOlly.dll` | Olly1/2（`TiDaojiOlly.ini`：DriverName/Type） |
| `TiDaojiTE.dll` | TitanEngine（`TiDaojiTE.ini`） |
| `install_driver.bat` | 复制 + `sc create/start`（**仍需**已在 DSE 窗口内） |

编译：VS2022 + WDK，打开 `TiDaoji.sln`。

---

## 3. 标准流程（画像 A）

```text
[可选] 卸 CR 等双 IH
  → 进入临时 DSE 窗口
  → 复制 TiDaoji.sys → sc create/start
  → 确认日志：InfinityHook armed / Hooks::Initialize
  → 立即 restore DSE   ← 不卸驱动
  → 验证 DSE 已恢复（不是验证 hook 消失）
  → x64dbg/GUI HidePid
  → 调试会话
  → 结束：Unhide → sc stop →（可选）sc delete
```

### 3.1 互斥预检

```bat
REM 列出可疑内核服务后人工 stop（名称因 CR 安装而异）
sc query type= driver state= all | findstr /i "CR Sak Infinity"
```

### 3.2 进入 DSE 窗口

```bat
cd /d D:\tools\kdu
kdu.exe -dse 0
REM 期望：DSE flags ... new value to be written: 0
```

### 3.3 安装并启动

**A. 脚本（仓库根）**

```bat
install_driver.bat
```

**B. 手写**

```bat
copy /Y TiDaoji.sys %SystemRoot%\system32\drivers\TiDaoji.sys
sc create TiDaoji binPath= %SystemRoot%\system32\drivers\TiDaoji.sys type= kernel
sc start TiDaoji
sc query TiDaoji
```

成功期望：

- `STATE` = RUNNING  
- 日志含类似：`Hooks::Initialize armed` / `InfinityHook hide armed` / `Start OK`  
- 失败：`reason=ConflictProbe` → 卸其它 IH 后重试  
- 失败：`Init FAIL build=… symbol=…` → 该 build pattern 未解析（记入矩阵，勿强行 fallback SSDT）

### 3.4 立即 restore DSE

```bat
cd /d D:\tools\kdu
kdu.exe -dse 6
REM 期望：value: 0 -> 6
```

**win-master 2026-08-07 实机记录**：start 后日志含 `InfinityHook hide armed` / `Hooks::Initialize armed`；`tools/tidaoji_smoke` 对 `\\.\TiDaoji` HidePid+UnhidePid 成功；DSE 已 restore 后服务仍 RUNNING。

**验收 DSE（示例思路，按工具改）：**

| 检查 | 期望 |
|------|------|
| 工具报告 DSE 已开 | 是 |
| 再 `sc start` 另一个**未**放行的未签名驱动 | 应失败（说明 DSE 回） |
| `TiDaoji` 仍 RUNNING | **是**（restore 不卸我们的驱动） |

**不要**把「系统干净 / 无 hook」当作 restore 成功标准。

### 3.5 用户态 Hide

**x64dbg（插件 v2）**

```
TiDaojiStatus          ; 探针 \\.\TiDaoji
TiDaojiOptions         ; 看 Type 位；默认约 0x7FF
TiDaoji                ; HidePid + 内置 hide
TiDaojiUnhide
TiDaojiUnhideAll
TiDaojiName NotTiDaoji ; 若 sc 服务名改过
TiDaojiHelp
```

自动：系统断点 → `TiDaoji`；结束调试 → `TiDaojiUnhide`。

**GUI**

1. Driver 名默认 `TiDaoji`（与服务/设备一致）  
2. 勾选 Type 位（会写入旁路 `.ini`）  
3. PID → Hide / Unhide / UnhideAll  

### 3.6 会话结束 / 回滚

```bat
REM 用户态先 Unhide（插件 TiDaojiUnhide / GUI）
sc stop TiDaoji
REM 可选：等数秒（驱动 Unload drain 默认 5s）
sc delete TiDaoji
del %SystemRoot%\system32\drivers\TiDaoji.sys
```

Unload 后仍建议确认无残留服务；**不要**在 inflight 调试中硬删。

---

## 4. 服务名变体（反字符串扫描）

```bat
sc create NotTiDaoji binPath= %SystemRoot%\system32\drivers\TiDaoji.sys type= kernel
sc start NotTiDaoji
```

x64dbg：`TiDaojiName NotTiDaoji`  
GUI：Driver 编辑框填 `NotTiDaoji`  

设备路径始终是 `\\.\` + 该名（实现上服务名与符号链接策略以驱动创建为准；默认实现为 `TiDaoji`）。

---

## 5. 故障速查

| 现象 | 动作 |
|------|------|
| CreateFile `\\.\TiDaoji` 失败 | `sc query`；DSE 是否挡装载；名称是否 `TiDaojiName` 一致 |
| start 失败 ConflictProbe | 卸 CR/其它 IH |
| start 失败 pattern/symbol | 记录 build；研究文档 §8；**勿**开 `TIDAOJI_ALLOW_SSDT_FALLBACK` 上生产 |
| Hide 无效 | `TiDaojiStatus`；Type 位；PID 是否当前调试进程 |
| restore 后仍担心 PG | 层 B 仍在是**预期**；只有 `sc stop` 才拆 hook 基础设施 |
| 睡眠后时间错 | 已知限制；避免长睡 |

---

## 6. 与「实机矩阵」的关系

| 本 runbook | 研究 §8 矩阵 |
|------------|----------------|
| 装载/restore/Hide **操作路径** | 多 build 兼容 **证据** |
| PR5 DoD | **最后**阶段；不阻塞 PR4/5 |

---

## 7. 检查清单（可打印）

- [ ] 已卸其它 InfinityHook（CR 等）  
- [ ] 进入 DSE 窗口  
- [ ] `sc start TiDaoji` 成功 + 日志 armed  
- [ ] **立即** restore DSE  
- [ ] 验证 DSE 恢复（非 hook 消失）  
- [ ] `TiDaojiStatus` / GUI 可打开设备  
- [ ] HidePid 后开始调试  
- [ ] 结束：Unhide → stop →（可选）delete  
- [ ] （最后）矩阵格另填，不在此强制  

---

**PR5 DoD**：照本文可独立完成装载与 restore 语义理解；工具命令行允许占位；不要求 24H2/VMP 实测结果。
