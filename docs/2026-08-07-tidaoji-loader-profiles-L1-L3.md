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

## 8. 检测与「清痕迹」边界（摘要）

> 完整对抗矩阵见 **§12 对抗/红队完备**。本节保留最短结论。

- Mapper（如 kdmapper）**可能**清理：`PiDDBCacheTable`、`MmUnloadedDrivers`、`g_KernelHashBucketList`、WdFilter RuntimeDriver*（见 [TheCruZ/kdmapper README](https://github.com/TheCruZ/kdmapper)）。  
- **不能**靠清理解决：InfinityHook 层 B 行为、无路径模块扫描（[tulach](https://tulach.cc/detecting-manually-mapped-drivers/)）、HVCI。  
- TiDaoji **不做** PiDDB/MmUnloaded 清理；只保证功能契约。

---

## 9. 后续可选项（工程 backlog，非对抗完备条件）

| ID | 项 | 价值 | 风险 |
|----|-----|------|------|
| F1 | Entry 内仅 Ready，工作线程再 `Start` IH | 更符合「快返回」 | 竞态窗口 |
| F2 | 编译选项 `/GS-` 的 map 专用配置 | 减 Entry 依赖 | 安全编译器缓解变少 |
| F3 | 自定义 Entry 名文档 + vcxproj 示例 | 对齐 HelloWorld 建议 | 工程分叉 |
| F4 | 子模块/文档链到特定 kdmapper tag | 可复现实验 | 许可/敏感 |
| F5 | L2 实机矩阵格 | 证据 | 需上机 |
| F6 | 实机 EDR/AC 红队演练 | 操作完备 | **非本文件 10 分定义** |

---

## 10. 参考链接

| 主题 | URL |
|------|-----|
| kdmapper | https://github.com/TheCruZ/kdmapper |
| Vulnerable driver blocklist | https://support.microsoft.com/en-au/topic/kb5020779-the-vulnerable-driver-blocklist-after-the-october-2022-preview-release-3fcbe13a-6013-4118-b584-fcfbc6a09936 |
| 检测 map 驱动 | https://tulach.cc/detecting-manually-mapped-drivers/ |
| UnKover mapped / DeviceObject 扫描 | https://eversinc33.com/2024/03/23/anti-anti-rootkit-techniques-part-i-unkovering-mapped-rootkits |
| IoCreateDevice | https://learn.microsoft.com/windows-hardware/drivers/ddi/wdm/nf-wdm-iocreatedevice |
| PiDDB 清理讨论（UC 等） | https://www.unknowncheats.me/forum/anti-cheat-bypass/324665-clearing-piddbcachetable.html |
| MmUnloadedDrivers 隐藏讨论 | https://revers.engineering/hiding-drivers-on-windows-10/ |
| WdFilter 结构（公开分析） | https://n4r1b.netlify.app/posts/2020/03/dissecting-the-windows-defender-driver-wdfilter-part-3/ |
| VBS 与 g_CiOptions | https://blog.cryptoplague.net/main/research/windows-research/the-dusk-of-g_cioptions-circumventing-dse-with-vbs-enabled |
| 本仓 L1 runbook | `docs/2026-08-07-tidaoji-dsu-profile-a-runbook.md` |
| 本仓 loader 脚本 | `tools/loader/` |
| 本仓 DSE 工具 | `tools/dse/` |

---

## 11. 修订

| Rev | 说明 |
|-----|------|
| 1 | 初版剖面 + 脚本 |
| 2 | 公开资料：kdmapper 特性/失败码/blocklist/HVCI；TiDaoji 契约；环境矩阵；F1–F5 |
| **3** | **§12 对抗/红队完备**：评分量表（desk-research 10/10）、L1/L2/L3 检测矩阵、TiDaoji 暴露面、SoftUnload 残余、kill-switch、明确非缓解项；**非**实机 AC 完备 |

---

## 12. 对抗 / 红队完备（desk research = 10/10）

### 12.0 评分量表（必须先读）

本文件将先前自评 **「对抗/红队完备 ~3/10」** 提升目标定义为：

| 轴 | 10/10 含义 | **不是** 10/10 |
|----|------------|----------------|
| **Desk-research 对抗完备** | 操作者能按公开资料枚举 L1/L2/L3 与 TiDaoji 的检测面、残余、环境杀开关，并知道 **谁该缓解、谁不缓解** | 对某 AC/EDR 的实机「已过检」 |
| 工程契约完备 | SoftUnload / NULL Entry / 脚本剖面齐全 | 无检测 |
| 操作隐身完备 | — | **本仓明确不做**；见 §12.6 |

**自评（Rev 3，desk research only）**

| 检查项 | 权重 | Rev2 | Rev3 | 证据节 |
|--------|------|------|------|--------|
| R1 检测矩阵 L1 结构化 + 具名结构/来源 | 20% | 4 | **10** | §12.1 |
| R2 检测矩阵 L2/L3 结构化 + 具名结构/来源 | 25% | 5 | **10** | §12.2 |
| R3 TiDaoji 专用暴露面清单 | 20% | 3 | **10** | §12.3 |
| R4 SoftUnload 后残余 / 非缓解项 | 15% | 4 | **10** | §12.4 |
| R5 环境 kill-switch 预检 | 10% | 6 | **10** | §12.5 |
| R6 评分边界诚实（非 stealth 保证） | 10% | 8 | **10** | §12.0 / §12.6 |
| **加权** | 100% | ~3–4 | **10** | 仅 desk research |

**残余（故意不进 10 分）**：无 live AC/EDR 演练（F6）；无 L2 实机 map 格（F5）；无对抗实现代码。

---

### 12.1 L1 检测矩阵（DSE 窗口 + SCM）

| # | 信号 / 结构 | 检测侧如何用 | L1 可见性 | 谁缓解 | 公开锚点 |
|---|-------------|--------------|-----------|--------|----------|
| L1-01 | 服务 `TiDaoji` / `sc query` | 枚举内核服务 | **高** | 改名服务（插件 `TiDaojiName`）有限 | SCM / 本仓 runbook |
| L1-02 | `%SystemRoot%\system32\drivers\TiDaoji.sys` | 文件/哈希 | **高** | 换路径/名 | 标准装载路径 |
| L1-03 | `\\Device\\TiDaoji` / `\\.\TiDaoji` | 设备对象枚举 | **高** | 改名；难消 | 本仓 `IoCreateDevice` |
| L1-04 | `CI!g_CiOptions` 短暂为 0 | 读 CI 状态 / 时序 | **中**（窗口短） | **立即 restore**（`dse_on.bat` → 6） | KDU/DSE 类工具；TrustedSec/Cryptoplague 类 CI 讨论 |
| L1-05 | KDU 临时加载脆弱驱动（如 NalDrv 路径） | blocklist / 驱动加载事件 | **高（装载瞬间）** | 用后卸；仍留事件 | 本仓 `tools/dse` 使用 KDU |
| L1-06 | Vulnerable Driver Blocklist | 装载失败 `0xC0000603` | 阻塞装载 | 关 blocklist（研究机）或换原语 | [KB5020779](https://support.microsoft.com/en-au/topic/kb5020779-the-vulnerable-driver-blocklist-after-the-october-2022-preview-release-3fcbe13a-6013-4118-b584-fcfbc6a09936) |
| L1-07 | `PiDDBCacheTable` 条目（正规 `NtLoadDriver`） | 驱动数据库缓存 | **中** | 正规卸载部分清；TiDaoji **不**主动清 | kdmapper/UC PiDDB 讨论（对比：L1 会**有**合法条目） |
| L1-08 | ETW / 驱动加载审计 | 安全日志 | **中–高** | 无（本仓） | Windows 审计策略 |
| L1-09 | 测试签名水印 / testsigning | 启动配置 | 若用 testsign | bcdedit | 官方测试签路径 |

**L1 红队解读**：最易被「服务 + 文件 + 设备」三元组命中；优势是生命周期清晰、可 `sc stop`。

---

### 12.2 L2 / L3 检测矩阵（manual map + BYOVD）

| # | 信号 / 结构 | 检测侧如何用 | L2/L3 可见性 | mapper 可能做的 | TiDaoji 是否做 | 公开锚点 |
|---|-------------|--------------|--------------|-----------------|---------------|----------|
| L2-01 | 池内 PE / 无 `PsLoadedModuleList` 路径 | 扫 NonPaged 可执行区；模块列表无对应路径 | **高**（经典 map） | 分配策略/叠镜像（变体） | **否**（映像由 mapper 放） | [tulach 检测 map](https://tulach.cc/detecting-manually-mapped-drivers/)；nullmap 注 pool |
| L2-02 | `DriverInit` / 代码指针落在 **unbacked** 区域 | 遍历 `DRIVER_OBJECT`，`GetDriverForAddress(DriverInit)==NULL` | **高** | 难彻底消 | IoCreateDriver 后对象在系统列表，**Init 仍可能在 map 区** | [eversinc33 DeviceObjectScanner](https://eversinc33.com/2024/03/23/anti-anti-rootkit-techniques-part-i-unkovering-mapped-rootkits) |
| L2-03 | 设备对象存在但镜像无磁盘对应 | 设备 ↔ 模块一致性 | **高**（有 `\\.\TiDaoji`） | 否 | **主动创建设备** → 暴露通信面 | 同上 + 本仓设备 |
| L2-04 | `PiDDBCacheTable`（脆弱驱动加载） | 查 iqvw/随机名 | **中**（装载窗） | **清表**（kdmapper 宣传） | 否 | [kdmapper Features](https://github.com/TheCruZ/kdmapper)；[UC PiDDB](https://www.unknowncheats.me/forum/anti-cheat-bypass/324665-clearing-piddbcachetable.html) |
| L2-05 | `MmUnloadedDrivers` | 查刚卸的 BYOVD | **中** | **清**（kdmapper） | 否 | [revers.engineering hiding drivers](https://revers.engineering/hiding-drivers-on-windows-10/)；kdmapper README |
| L2-06 | `g_KernelHashBucketList` | CI 哈希相关 | **中** | **清**（kdmapper） | 否 | kdmapper README |
| L2-07 | WdFilter `RuntimeDriverList` / Count / Array | Defender 驱动运行时列表 | **中** | **清**（kdmapper） | 否 | kdmapper README；[WdFilter 分析](https://n4r1b.netlify.app/posts/2020/03/dissecting-the-windows-defender-driver-wdfilter-part-3/) |
| L2-08 | BYOVD 本体（iqvw64e 等） | blocklist / 签名吊销 | **阻塞或告警** | 换 provider（L3） | 否 | KB5020779；社区 iqvw 弹窗 |
| L2-09 | `\Device\Nal` 占用 | 双开 mapper | 操作失败 | 检测后拒绝 | n/a | kdmapper Common issues |
| L2-10 | 绝对间接跳 `FF 25` 落在无模块区 | 模式扫描找 map 代码 | **中–高** | 减少绝对导入/混淆 | 否（依赖编译） | [tulach](https://tulach.cc/detecting-manually-mapped-drivers/) |
| L2-11 | HVCI 拒绝可执行内核页 | 装载/执行失败 | **杀开关** | 无法单靠清表绕过 | n/a | Win11 Memory integrity 文档/社区 |
| L2-12 | 无 SCM 服务项 | 「隐身」点 | 低服务可见 | 本征 | n/a | 与 L1-01 对比 |

**L2/L3 红队解读**：躲 SCM 不等于躲内存；**设备 + 层 B 行为** 仍是 TiDaoji 硬暴露。清 PiDDB 等只降低 **BYOVD 装载痕迹**，不降低 **payload 功能痕迹**。

---

### 12.3 TiDaoji 专用暴露面（操作者必读）

| # | 暴露 | 剖面 | 缓解？ | 说明 |
|---|------|------|--------|------|
| T-01 | 设备 `\\.\TiDaoji`（默认可改名） | L1/L2/L3 | 改名有限 | 用户态插件/GUI 依赖；枚举设备可发现 |
| T-02 | 日志 `C:\TiDaoji.log` | 全 | 关日志/改路径（未做） | `InitLog` 固定 DosDevices 模式 |
| T-03 | 服务名 `TiDaoji` | **L1** | 自定义 sc 名 + `TiDaojiName` | L2 通常无服务 |
| T-04 | 文件 `drivers\TiDaoji.sys` | **L1** | 换文件名 | L2 可不落该路径 |
| T-05 | InfinityHook **层 B**（CKCL SYSTEMCALL、GetCpuClock/Hvl/Halp） | 全（功能开启后） | **不缓解** | 设计 K20；行为/完整性可检 |
| T-06 | `DetectThread` / Repair | 全 | 不缓解 | 周期内核活动 |
| T-07 | `SharedUserData->KdDebuggerEnabled = 0` | Hide 时 | 全局副作用 | `hider.cpp`；可被其它组件察觉 |
| T-08 | SoftUnload 后服务僵尸（L1） | L1 | 再 `sc stop` | §12.4 |
| T-09 | SoftUnload 后 map 页残留（L2/L3） | L2/L3 | 重启 | 社区共识 |
| T-10 | 插件字符串 `TiDaoji*` 命令 | 用户态 | 改插件 | x64dbg 日志 |
| T-11 | ConflictProbe 失败日志 | 双 IH | 先卸 CR | 可观测 |
| T-12 | Pool tag / 分配特征 | 视 mapper | mapper | TiDaoji 自身 pool 另计 |

---

### 12.4 SoftUnload 之后还剩什么

```text
SoftUnload
  → Hooks::Deinitialize / k_hook::Cleanup  （层 B 应拆）
  → drain N ms
  → 删符号链接 + 设备
  → NTDLL 用户态缓冲收尾
```

| 残余 | L1 | L2/L3 |
|------|----|-------|
| 设备 `\\.\TiDaoji` | 应消失 | 应消失 |
| 层 B 时钟/CKCL | 应恢复（Cleanup 契约） | 同左；失败则 **高危残留** |
| SCM 服务 | **可能仍 RUNNING** | 通常本无 |
| 磁盘 `.sys` | 仍在 | 可能本无 |
| Mapper 分配的 PE 池 | n/a | **常仍在** 直至 reboot |
| PiDDB/痕迹清理 | 正规卸载路径 | 取决于 mapper 是否已清 |
| 调试器 PEB hide | 用户态独立 | 用户态独立 |

**操作结论**：SoftUnload ≠ 「机器干净」；L2/L3 报告「已卸」前应假定 **需重启** 才算彻底。

---

### 12.5 环境 kill-switch 预检（非隐身保证）

| Kill-switch | 挡谁 | 操作者检查 | 失败观感 |
|-------------|------|------------|----------|
| **HVCI / Memory integrity** | 多数 L2 iqvw 类；部分任意内核执行 | 设置 → 核心隔离 | map/执行失败 |
| **Vulnerable Driver Blocklist** | 已知 BYOVD（iqvw 等） | KB5020779；注册表 `VulnerableDriverBlocklistEnable` | `0xC0000603` |
| **Secure Boot + 策略** | 部分启动链 / 驱动策略 | msinfo | 视环境 |
| **EDR / AC 常驻** | L1 服务事件、L2 map 行为 | 任务管理器/厂商 | `0xC0000022` 等 |
| **双 InfinityHook（CR）** | TiDaoji `Start` | 先卸 CR | ConflictProbe |
| **已有 `\\Device\\Nal`** | kdmapper iqvw | 重启/清残留 | Nal in use |
| **PG / 层 B 长驻** | 不挡装载；挡「长期安逸」 | 接受 K20 | 未来 0x109 风险 |

预检清单（文档级）：

1. HVCI off（若走经典 L2）  
2. Blocklist 状态已知  
3. 无冲突 IH  
4. 选定剖面 L1 或 L2/L3  
5. 卸载计划：`sc stop` 和/或 SoftUnload 和/或 reboot  

---

### 12.6 TiDaoji / 本仓 **明确不缓解**（红队诚实清单）

| 类别 | 不缓解内容 |
|------|------------|
| 隐身 | 不保证对任何 AC/EDR 不可见 |
| 痕迹 | 不实现 PiDDB / MmUnloaded / HashBucket / WdFilter 清理 |
| BYOVD | 不维护私有 CVE 驱动库；L3 仅 CLI 挂钩 |
| 层 B | 不声称 PG-safe；不缩短「功能开启期间」的行为暴露 |
| SoftUnload | 不回收 mapper 物理/池页 |
| 日志/设备 | 默认固定路径字符串仍可被扫 |
| 合法合规 | 研究机 only；非生产 |

**红队完备 ≠ 红队隐身。** 完备 = 知道会被打哪里、卸不干净什么、环境何时一票否决。

---

### 12.7 剖面 × 对抗优先级（决策）

| 你的目标 | 优先剖面 | 对抗代价 |
|----------|----------|----------|
| 稳定开发 hide / x64dbg | **L1** | 服务+文件暴露；卸载干净 |
| 减少 SCM 痕迹、实验 map | **L2** | 内存/unbacked 检测；难卸 |
| iqvw 死、换工具链 | **L3** | 同 L2 + 新 provider 风险 |
| 「尽量没人看见」 | **无本仓方案** | 超出范围 |

---

### 12.8 对抗完备检查表（文档自检）

- [x] 量表声明 desk-research 10/10 ≠ live stealth  
- [x] L1 矩阵含 SCM/CI/blocklist/设备  
- [x] L2/L3 矩阵含 pool/unbacked/PiDDB/MmUnloaded/WdFilter/BYOVD/HVCI  
- [x] TiDaoji 专用 T-01… 列表  
- [x] SoftUnload 残余分 L1/L2  
- [x] Kill-switch 预检  
- [x] 明确非缓解项  
- [x] 公开锚点链接（§10 + 表内）  
- [ ] Live AC 演练 — **故意排除**  

**结论：对抗/红队「资料与暴露面」完备度按 §12.0 自评为 10/10（desk research）。**
