# TiDaoji 装载剖面 L1 / L2 / L3（资料完善版）

| 字段 | 值 |
|------|-----|
| **日期** | 2026-08-07（Rev 2：公开资料完善，**未**上实机验证 L2/L3） |
| **状态** | Research + 仓库可选脚本；驱动契约已实现 |
| **驱动契约** | `DriverObject==NULL` → `IoCreateDriver`；`SoftUnload` |
| **明确不做** | 树内 exploit/PoC；保证任意环境 map 成功；PG-safe |

> **NOT PG-safe.** 剖面只解决「如何进入内核」，不改变 InfinityHook 层 B 风险。  
> L2/L3 调用**外部** mapper；不把脆弱驱动利用实现写进 TiDaoji 业务代码。

---

## 0. 两条大路（先分清）

| 路线 | 本质 | 本仓剖面 |
|------|------|----------|
| **正规装载器 + 临时签名策略口子** | 改 CI / 测试签 后 `NtLoadDriver`/`sc` | **L1**（`tools/dse`） |
| **Manual map** | 内核任意写 → 铺 PE → 调 Entry；**不**走完整 SCM 装载 | **L2/L3** |

公开实现代表：[TheCruZ/kdmapper](https://github.com/TheCruZ/kdmapper)（由 z175 原版改进；README 自称测试范围约 **Win10 1607 → Win11 25H2** 某 build，**以你本机为准**）。

二次文献（机制/检测，非教程 exploit）：

- GuidedHacking / UC 论坛：manual map 无真实 `DRIVER_OBJECT` 的讨论  
- eversinc33 *UnKovering mapped rootkits*：map 痕迹与 PiDDB 等  
- tulach.cc *Detecting manually mapped drivers*：检测面  
- Microsoft KB5020779：Vulnerable Driver Blocklist  
- Cryptoplague 等：VBS 时代 `g_CiOptions` 局限（主要影响 **L1 改 CI**，不是 map 本身）

---

## 1. 剖面总表

| 剖面 | 手段 | 入口脚本 | 卸载 | 何时选 |
|------|------|----------|------|--------|
| **L1** | KDU 等改 DSE → `sc start` | `tools/dse/load_tidaoji_profile_a.bat` | `sc stop`（或 SoftUnload 后仍建议 sc） | 默认真机研究（19045 已验证 L1） |
| **L2** | 外部 kdmapper 类手映 | `tools/loader/L2_kdmapper.bat` | **SoftUnload**；池映像常需重启才净 | 不想/不能改 CI；接受无 SCM |
| **L3** | 换 BYOVD provider / 自定义 mapper CLI | `tools/loader/L3_multi_provider.bat` | 同 L2 | iqvw 被拦、AV/AC、多环境 |
| **Auto** | 环境变量探测 | `tools/loader/load_auto.bat` | 依所选剖面 | 一键 |

---

## 2. kdmapper 机制摘要（资料）

### 2.1 流水线

```text
用户态 mapper
  → 加载已签名脆弱驱动（经典：Intel iqvw64e.sys / 其它 provider）
  → IOCTL：内核/物理读写原语
  → 内核分配 + 拷贝 PE（可跳过 header）
  → 重定位 / 导入解析
  → 调用自定义入口（常不是完整 I/O 管理器装载）
  → （可选）清痕迹结构
  → 卸脆弱驱动
```

### 2.2 README 级「Features」（TheCruZ 公开说明）

| 能力 | 含义（对 TiDaoji） |
|------|-------------------|
| 清 `MmUnloadedDrivers` | 减卸载列表痕迹（mapper 侧） |
| 清 `PiDDBCacheTable` | 减驱动数据库缓存痕迹 |
| 清 `g_KernelHashBucketList` | 哈希桶痕迹 |
| 清 WdFilter RuntimeDriver* | Defender 驱动列表相关 |
| `\Device\Nal` 占用检测 | 防双重 iqvw / 残留 BSOD |
| `--free` | map 后释放分配（**仅适合瞬时执行**；**不适合**长驻 IH） |
| `--PassAllocationPtr` | 改 Entry 第一参语义 |
| PDB offsets | 跟 build 漂移；过期 → BSOD 风险 |

### 2.3 对 **被 map 驱动** 的硬要求（公开共识）

1. **`DriverObject` / `RegistryPath` 默认可为 NULL**（除非 mapper 显式构造）。  
2. **Entry 尽快返回**；禁止 Entry 内死循环（否则 mapper 卡死 / 超时观感）。长驻用线程/工作项。  
3. **不能依赖 SCM Unload**；社区常说「干净卸载 ≈ 重启」，或自写释放+拆 hook（易漏）。  
4. 若要用 `IoCreateDevice`，必须先有**有效** `DRIVER_OBJECT` → 本仓用 **`IoCreateDriver`** 解决 NULL Entry。  
5. 推荐自定义入口点（避免 `/GS` 的 `GsDriverEntry` 在无 CRT 初始化时的坑）—— VS 驱动工程注意 Entry 符号。

### 2.4 常见失败码 / 现象（资料 + issue）

| 现象 | 常见原因 | 剖面动作 |
|------|----------|----------|
| `0xC0000603` STATUS_IMAGE_CERT_REVOKED | 脆弱驱动在 **Vulnerable Driver Blocklist** | 关 blocklist（注册表 `VulnerableDriverBlocklistEnable=0` + 重启）或 **L3 换 provider** |
| `0xC0000022` / `0xC000009A` | AV / 反作弊拦截 | 隔离环境；换机 |
| `\Device\Nal already in use` | iqvw 残留 / 双开 mapper | 重启；清 %TEMP% 随机名驱动 |
| pattern / offset fail | 新 build | 更新 mapper / PDB offsets |
| HVCI / Memory integrity | 内存完整性 | 研究机关 HVCI；否则 L2 常死 |
| map 成功但无设备 | Entry 失败 / 未 IoCreateDriver | 查 `C:\TiDaoji.log`；确认本仓新 sys |

**Microsoft Vulnerable Driver Blocklist**（KB5020779）使经典 **iqvw64e** 在很多 Win11/24H2 上直接不可用 —— 这是推 **L3 多 provider** 的主要理由，不是「再改一次 g_CiOptions」能单独解决。

---

## 3. TiDaoji 已实现契约 vs 资料要求

| 要求 | TiDaoji 状态 |
|------|----------------|
| NULL `DriverObject` | **已**：`IoCreateDriver(L"\\Driver\\TiDaoji", DriverInitialize)` |
| NULL `RegistryPath` | **已**：设备名回落 `TiDaoji` |
| 设备 `\\.\TiDaoji` | **已**：`IoCreateDevice` + 符号链接 |
| 快返回 Entry | **部分**：Entry 内同步 `Hooks::Initialize`（pattern+CKCL+时钟）可能 **偏慢**；资料建议「越快越好」。**未实机压测**。后续可选：工作线程延迟 `Start`（实验开关） |
| 卸载 | **SoftUnload** 拆逻辑；**不**回收 mapper 分配的整镜像池 → 与资料「难净卸」一致 |
| `/GS` | 沿用 WDK 默认；若 map 异常再试 `/GS-` 或自定义 Entry |

### SoftUnload 语义（仓库实现）

```text
WriteFile(\\.\TiDaoji, HIDE_INFO{ SoftUnload, 0, 0 })
→ FullTeardown: IH Cleanup + drain + 删设备/符号链接 + NTDLL 收尾
```

| 路径 | SoftUnload 后 |
|------|----------------|
| L1（有服务） | 设备没了，**服务可能仍 RUNNING** → 应 `sc stop/delete` |
| L2/L3 | 逻辑停；**映像页可能仍在** → 彻底干净常 **重启** |

插件：`TiDaojiSoftUnload`  
脚本：`tools/loader/soft_unload.bat`

---

## 4. 多环境选型矩阵（资料向，非实测承诺）

| 环境因素 | 更可能可用 | 备注 |
|----------|------------|------|
| Win10 1903–22H2，HVCI off，blocklist off | L1 或 L2 | L1 已在 19045 冒烟 |
| Win11 + **Vulnerable Driver Blocklist on** | L1（KDU 其它原语）或 **L3 非 iqvw** | iqvw 常 0xC0000603 |
| **HVCI / Memory integrity on** | 往往全军覆没 | 研究机关闭 |
| VBS 强策略 | L1 改 `g_CiOptions` 变难 | 见 VBS/DSE 文献；map 另论 |
| 有企业 EDR | L2/L3 痕迹清理仍可能被行为检 | 隔离 VM |
| 仅要稳定调试 TiDaoji | **L1** | 契约完整、可 sc stop |
| 要无服务名 / 实验 map | **L2** | 接受重启卸载 |
| iqvw 死、有私有 mapper | **L3** | 只改环境变量 |

---

## 5. L2 操作清单（文档级，不上机）

```bat
:: 1) 外部准备 kdmapper（或兼容 CLI），自备脆弱驱动策略
set KDMAPPER=C:\lab\kdmapper.exe
set TIDAOJI_SYS=C:\build\TiDaoji.sys

:: 2) 环境预检（人工）
::    - HVCI off
::    - VulnerableDriverBlocklistEnable 按需
::    - 无残留 \Device\Nal / 双开 mapper

:: 3) map
tools\loader\L2_kdmapper.bat

:: 4) 功能
tools\tidaoji_smoke.exe
:: 或 x64dbg TiDaojiStatus / TiDaoji

:: 5) 结束
tools\loader\soft_unload.bat
:: 彻底干净：reboot
```

**不要**对长驻 TiDaoji 使用 mapper 的 `--free`（会拆掉映像）。

---

## 6. L3 操作清单

```bat
set TIDAOJI_MAPPER=C:\lab\your_mapper.exe
set TIDAOJI_PROVIDER=0
set TIDAOJI_MAPPER_ARGS=
set TIDAOJI_SYS=C:\build\TiDaoji.sys
tools\loader\L3_multi_provider.bat
```

`providers.example.ini`：占位模板，**不**提交真实 exploit 驱动路径到公开远端时注意脱敏。

L3 与 L2 对 TiDaoji **驱动契约相同**；差别只在 **用户态用哪条 BYOVD/map 工具链**。

---

## 7. L1 仍建议保留的原因（资料 + 工程）

| 点 | L1 | L2/L3 |
|----|----|-------|
| `DRIVER_OBJECT` 生命周期 | 完整 | 依赖 IoCreateDriver / 假对象 |
| 日常改代码迭代 | `sc stop/start` | 常 reboot |
| 与 PR3 Unload drain | 对齐 | SoftUnload 近似 |
| 检测 | 服务 + CI | 池模块 + 清理痕迹 |
| 公开稳定性叙事 | 运维简单 | iqvw 持续被 blocklist 打 |

**策略**：主开发 **L1**；map **L2/L3 实验轨**，用同一 `TiDaoji.sys`。

---

## 8. 检测与「清痕迹」边界（资料）

Mapper 可能清理：

- PiDDBCacheTable  
- MmUnloadedDrivers  
- KernelHashBucketList  
- WdFilter RuntimeDriver*  

**不能**靠清理解决：

- InfinityHook 层 B（CKCL/GetCpuClock/Hvl）行为异常  
- 无路径内核模块的内存完整性扫描（见 tulach 等检测文）  
- HVCI 下的代码完整性  

TiDaoji **不做** PiDDB 清理（留给 mapper）；本仓只保证功能契约。

---

## 9. 后续可选项（未做，文档登记）

| ID | 项 | 价值 | 风险 |
|----|-----|------|------|
| F1 | Entry 内仅 Ready，工作线程再 `Start` IH | 更符合「快返回」 | 竞态窗口 |
| F2 | 编译选项 `/GS-` 的 map 专用配置 | 减 Entry 依赖 | 安全编译器缓解变少 |
| F3 | 自定义 Entry 名文档 + vcxproj 示例 | 对齐 HelloWorld 建议 | 工程分叉 |
| F4 | 子模块/文档链到特定 kdmapper tag | 可复现实验 | 许可/敏感 |
| F5 | L2 实机矩阵格 | 证据 | 需上机 |

---

## 10. 参考链接

| 主题 | URL |
|------|-----|
| kdmapper | https://github.com/TheCruZ/kdmapper |
| Vulnerable driver blocklist | https://support.microsoft.com/topic/kb5020779-… |
| 检测 map 驱动 | https://tulach.cc/detecting-manually-mapped-drivers/ |
| UnKover mapped | https://eversinc33.com/2024/03/23/anti-anti-rootkit-techniques-part-i-unkovering-mapped-rootkits |
| IoCreateDevice | https://learn.microsoft.com/windows-hardware/drivers/ddi/wdm/nf-wdm-iocreatedevice |
| 本仓 L1 runbook | `docs/2026-08-07-tidaoji-dsu-profile-a-runbook.md` |
| 本仓 loader 脚本 | `tools/loader/` |

---

## 11. 修订

| Rev | 说明 |
|-----|------|
| 1 | 初版剖面 + 脚本 |
| 2 | 公开资料：kdmapper 特性/失败码/blocklist/HVCI/痕迹清理；TiDaoji 契约对照；环境矩阵；F1–F5  backlog；**不上实机** |
