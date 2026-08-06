# InfinityHook 族谱与新系统兼容研究

| 字段 | 值 |
|------|-----|
| **日期** | 2026-08-06（落地修订 2026-08-07） |
| **状态** | **Landed in-repo** — 文档 + 代码缓解已合入 `pr3/wire-infinity-hook`；live 支持矩阵 **env-blocked** |
| **主目标** | 评估 [ThomasonZhao/InfinityHookProMax](https://github.com/ThomasonZhao/InfinityHookProMax) 对新系统的兼容声明；对照 TiDaoji 当前引擎与残余风险 |
| **资料来源** | `gh` API + 源码；Web 公开分析；本地 `CR_Full/SakDriver/infinity_hook_pro`；`TiDaoji/infinity_hook` |
| **关联设计** | `docs/2026-08-06-tidaoji-infinityhook-design.md`（Rev 6） |
| **门禁脚本** | `tools/verify_research_landed.sh` |
| **读者** | TiDaoji 驱动研发；Claude 补充见 §10.2 |

> **硬结论先行**：InfinityHookProMax **不是** 2024–2026 新系统的权威维护仓；实测证据停在 **Win11 22000 + VM**。TiDaoji 当前引擎（CR 系移植 + PR2 生命周期）在 magic / 物理机 / 冲突 / 生命周期上 **严格强于 IHPM**。新系统与物理机应对照 **zhutingxf / CR**，而不是整仓替换成 ProMax。  
> **禁止**：整仓 vendor IHPM；生产开 `TIDAOJI_ALLOW_SSDT_FALLBACK`；任何 **PG-safe** 声称；伪造 24H2 矩阵通过格。

---

## 1. 范围与方法

### 1.1 要回答的问题

1. InfinityHookProMax 对 Win11 新 build（22H2 / 23H2 / 24H2 / 25H2…）的兼容是否成立？
2. 它相对 everdox / FiYHer / 物理机 fork / CR / TiDaoji 处在族谱的哪一层？
3. 对 TiDaoji 五条残余风险有什么可操作参考（采纳 / 拒绝 / 仅数字）？
4. 下一步 pattern 矩阵与可选补丁应优先抄谁？

### 1.2 方法

| 手段 | 用途 |
|------|------|
| `gh repo view` / `gh api .../contents` / commits / issues | 元数据、README、hook.cpp 全文、issue |
| Web 检索 | ETW/GetCpuClock 现代路径、公开分析 |
| 本地 CR + TiDaoji 源码 `rg` | 差分：magic、Halp、生命周期、ConflictProbe |
| 外部模型补充 | 文末 §10 Claude 补充区 |

### 1.3 刻意不做的事

- 不声称任何 InfinityHook 变体 **PG-safe**。
- 不把 README 的 “Win11:latest” 当作测试证据。
- 不建议把 ProMax 整仓 vendor 进 TiDaoji。

---

## 2. 族谱位置

```
everdox/InfinityHook          (经典 GetCpuClock 函数指针时代)
        │
        ├─ fIappy/infhook19041
        ├─ huoji120/MakeInfinityHookGreatAgain   (2004 / selector=2 + Hvl)
        │
        ▼
FiYHer/InfinityHookPro        (Win7→Win11 偏移整理；实测至 22000)
        │
        ├─ ThomasonZhao/InfinityHookProMax   ★ 本文主评对象
        │     · 2023-08 英文整理 / MIT
        │     · README: VM only, “Win11:latest”
        │     · 实测列表: … Win11 22000
        │     · Issue: physical machines 不工作
        │
        └─ zhutingxf/InfinityHookPro
              · 物理机 + 22621+ / 24H2 截图与 README 声明
              · 睡眠唤醒 QPC 时间漂移 bug 自述
              · 2024 仍有提交（24H2 截图）

CR SakDriver/infinity_hook_pro
        · 与 zhutingxf 同代物理机路径 + 601802
        · 带 IsStarted 等（仍有生命周期坑，TiDaoji PR2 修）

TiDaoji/infinity_hook (PR2/PR3)
        · Port of CR + K13–K19（Ready/Start/Stop/Cleanup/Repair/ConflictProbe）
        · 生产 hide 经 PR3 接线；SSDT 写路径冻结
```

### 2.1 仓库快照（gh，2026-08-06 查询）

| 仓库 | stars | 有效代码窗口 | 官方支持措辞 | 实测/截图证据 |
|------|-------|--------------|--------------|---------------|
| [ThomasonZhao/InfinityHookProMax](https://github.com/ThomasonZhao/InfinityHookProMax) | ~53 | **2023-08** 最后逻辑提交 | **全部 VIRTUAL MACHINES** Win7→Win11:latest | Win7, Win8, 1909, 21h1, **22000** |
| [FiYHer/InfinityHookPro](https://github.com/FiYHer/InfinityHookPro) | ~560 | 2023 初为主 | “理论上 Win7→最新 Win11” | 同列表；Issue 含物理机/22H2/KB 特征码 |
| [zhutingxf/InfinityHookPro](https://github.com/zhutingxf/InfinityHookPro) | ~114 | 2024-01 代码；2024-10 24H2 图 | VM **与物理机**；22621+ | 截图含 22621/22631/26016/**24H2** |
| everdox/InfinityHook | 经典 | 早期 | “与 PG/VBS 并存” 的历史表述 | 现代 build 经典指针路径已失效 |
| 本地 CR `infinity_hook_pro` | n/a | 项目内 | 物理机 + 现代 magic | TiDaoji 移植源 |
| TiDaoji `infinity_hook` | n/a | PR2/PR3 | 设计文档 K20：NOT PG-safe | 待支持矩阵实测 |

### 2.2 IHPM Issue（直接相关）

| # | 标题 | 状态 | 含义 |
|---|------|------|------|
| 1 | Does not work on physical machines | OPEN | 与 README “VM only” 一致；**不要**拿 IHPM 做物理机基线 |
| 2 | NtTraceControl failing when manual mapped | OPEN | 与 DSU/手动映射装载相关；TiDaoji 正常 sc 装载路径不同但仍要注意 CKCL |

---

## 3. InfinityHookProMax 技术解剖

源：`infinity_hook_pro_max/.../hook.cpp`（约 445 行）、`main.cpp`、`utils.hpp`。

### 3.1 API 面

```text
k_hook::initialize(callback)  // 开 CKCL + pattern + 解析槽
k_hook::start()               // 改时钟 + detect 线程
k_hook::stop()                // 停 detect + 恢复时钟 + 折腾 CKCL stop/start
```

**相对 TiDaoji 的缺陷**：

- `initialize` 阶段即 `start_trace` / 开 SYSTEMCALL 语义，**无 Ready/Running 分离**。
- **无** `ConflictProbe` / `LayerBIntact` / `Repair` / `Cleanup` 幂等语义。
- Detect 线程仅对 **build ≤ 18363** 的指针路径做 re-init；**现代 Hvl 路径几乎不修**。

### 3.2 版本分支（与族谱共识）

| Build 条件 | 行为 |
|------------|------|
| `≤ 7601` | KTHREAD call_index 偏移 `0x1f8`；GetCpuClock 槽 **+0x18** |
| `7601 < b < 22000` 且非上述 | GetCpuClock 槽 **+0x28**（Win8–Win10） |
| `≥ 22000` | GetCpuClock 槽 **+0x18**（Win11） |
| `≤ 18363` | `*GetCpuClock = self_get_cpu_clock`（函数指针时代） |
| `> 18363` | `*GetCpuClock = (void*)2` + 写 `HvlGetQpcBias` |

### 3.3 Pattern 集合（IHPM）

| 目标 | Pattern / 策略 | 备注 |
|------|----------------|------|
| EtwpDebuggerData | `\x00\x00\x2c\x08\x04\x38\x0c` mask `??xxxxx` | `.text` → `.data` → `.rdata` |
| HvlpReferenceTscPage | `48 8b 05 ?? ?? ?? ?? 48 8b 40 ?? 48 8b 0d ?? ?? ?? ?? 48 f7 e2` | `>18363` |
| HvlGetQpcBias 主 | `48 8b 05 ??… 48 85 c0 74 ?? 48 83 3d … 74` | 旧 |
| HvlGetQpcBias 备 | `48 8b 05 ??… e8 ??… 48 03 d8 48 89 1f` | **KB5018410** / FiYHer #17 |
| 栈 magic | **仅** `0x501802` + `0xF33` | **无 0x601802** |
| 栈目标 | `stack_current[9]` | 与经典一致 |
| SSDT 页窗 | `PAGE_ALIGN(syscall_entry)` 起 2 页 | LSTAR + KVASCODE 解 shadow |

### 3.4 Unload

`main.cpp`：`stop()` 后 **10s** `KeDelayExecutionThread`，注释写 *can be improved*。  
**无** per-syscall 引用计数 —— 与 TiDaoji 同属“固定 drain 降 UAF 概率”。

### 3.5 Demo 回调反模式

```text
g_NtCreateFile = MmGetSystemRoutineAddress(L"NtCreateFile");
// 回调里 *ssdt_address == g_NtCreateFile 才替换
```

**问题**：导出地址与 **SSDT 槽** 指向可能不一致 → 静默不 hook。  
TiDaoji 设计（K15）强制 `SSDT::GetFunctionAddress`，**正确**。

### 3.6 兼容性判定（IHPM 自身）

| 声明 | 证据等级 | 判定 |
|------|----------|------|
| VM Win7–Win10 21h1 | 作者测试列表 | 可信为历史基线 |
| VM Win11 22000 | 作者测试列表 | 可信；偏移 0x18 是关键修复 |
| VM Win11 “latest” / 24H2 | **无** commit / 无截图 / 无 issue 关闭证明 | **不成立** |
| 物理机 | Issue #1 OPEN + README 排除 | **不支持** |
| PG-safe | 未声称；层 B 长驻 | **不得**外推为安全 |

---

## 4. 现代 Windows 上的机制现实（公开研究摘要）

> 用于解释“为什么还在改 Hvl/Halp”，不是 IHPM 独有。

1. **经典路径死亡**：早期 `WMI_LOGGER_CONTEXT.GetCpuClock` 可塞任意函数指针；现代内核多为 **clock selector**（0/1/2/3…），直接塞指针不再得到可控回调。
2. **存活路径**：强制 selector=2 → 走 `HalpTimerQueryHostPerformanceCounter` → 下游 **可改写** 的 `HvlGetQpcBias`（及物理机上额外 Halp type / Hvlp 空指针补丁）。
3. **仍非 PG 形式化安全**：改的是 ETW/HAL 热路径上的全局指针与类型字段；与“不写 SSDT 表”只消除 **一类** 0x109，不消除层 B 风险。
4. **CU/KB 会打断特征码**：KB5018410 类案例已在 FiYHer issue 复现；24H2/25H2 只能 **实测**，不能读 2023 README。

公开可读参考（检索时点 2026-08）：

- everdox/InfinityHook README（经典原理）
- freebuf / 看雪 / HITCON 幻灯：19041 后 Hvl 路径
- the-deniss：Win11 22H2 类 Avast 自保护与 syscall ETW 路径
- kernullist 等 2025–2026 ETW 深潜：selector 与下游 hook 仍存、直接指针时代结束

---

## 5. 四路实现对照表

| 能力 | IHPM (Thomason) | FiYHer | zhutingxf | CR / TiDaoji |
|------|-----------------|--------|-----------|--------------|
| GetCpuClock 0x18@≥22000 | ✓ | ✓ | ✓ | ✓ |
| selector=2 + Hvl | ✓ | ✓ | ✓ | ✓ |
| Hvl 双 pattern (KB) | ✓ | ✓ | ✓（注释按 22H2/22621 分段） | ✓ |
| 栈 magic **601802** | ✗ | ✗（早期） | ✓（注释：Win11 **23606+**） | ✓ |
| 物理机 Halp type=5→VM type | ✗ | ✗ | ✓ | ✓（TiDaoji 已移植） |
| HvlpGetReferenceTime 空指针 fake | ✗ | ✗ | ✓ | ✓ |
| QPC 睡眠修正尝试 | ✗ | ✗ | ✓（仍承认睡眠后时间错） | ✓（有 Qpc MDL 路径） |
| ConflictProbe | ✗ | ✗ | ✗ | ✓ **TiDaoji only（PR2）** |
| Ready≠Start / Cleanup | ✗ | ✗ | ✗ | ✓ TiDaoji |
| Detect 修现代层 B | 弱（≤18363） | 弱 | 偏旧 detect | ✓ Repair |
| Unload drain | 10s | 类似 | 类似 | **5s** 默认（可配至 10s） |
| 匹配用 SSDT 槽 | demo 用导出 | 视 demo | 视 demo | ✓ 强制 |
| 声称 24H2 | 否（虚 latest） | 虚 | **是（截图+README）** | 待自测 |
| 声称物理机 | **否** | issue 差 | **是** | 路径在；待矩阵 |
| 最后认真维护 | 2023 | ~2023 | 2024 | 本仓库 2026 |

### 5.1 栈 magic 注释（zhutingxf / CR / TiDaoji）

```text
0x501802  — Win11 23606 以前常见
0x601802  — Win11 23606 及以后
0xF33     — 第二 magic（short）
```

**IHPM 只有 0x501802** → 在 23606+ 上可能 **静默漏 hook**（装得上但不进回调）。  
TiDaoji 已双 magic —— **禁止回退到 IHPM 单 magic**。

### 5.2 物理机路径要点（zhutingxf / CR / TiDaoji）

```text
HALP_PERFORMANCE_COUNTER_TYPE_OFFSET = 0xE4
TYPE_PHYSICAL_MACHINE = 5
VM type: 22000 及以下常见 8；22621+ 常见 7（从指令中抠立即数，勿写死）

仅当 type==5：
  - 分配 fake HalpOriginalPerformanceCounter 副本（rate + type）
  - 改 *HalpPerformanceCounterType 为 VM 侧期望值
  - 若 HvlpGetReferenceTimeUsingTscPage == 0，挂 Fake（返回 rdtsc）
  - 可选：映射 KUSER_SHARED_DATA QpcBias 供 Stop 时睡眠漂移修正
```

**副作用（zhutingxf 自述）**：hook 期间长睡眠再唤醒 → **系统时间错误**；Stop 修正不完美。  
TiDaoji 运维含义：研究机避免 S3/S4 长睡；不要当“时间源可靠”。

---

## 6. 对照 TiDaoji 五条残余风险

| # | 风险 | IHPM 参考 | zhutingxf/CR 参考 | TiDaoji 现状 | 建议 |
|---|------|-----------|-------------------|--------------|------|
| 1 | 层 B 长驻 → NOT PG-safe | 同样长驻；不谈 PG | 48h 物理机“未触发”**传闻** | K20 接受 | **不降级风险文案**；矩阵冒烟≠PG 证明 |
| 2 | 新 build pattern 失效 | 证据停 22000；KB 第二 pattern | 24H2 截图；特征分段注释更细 | 硬失败 + `reason=` | **以 zhutingxf/CR 为 pattern 补丁源**；矩阵必含 24H2 |
| 3 | 与 CR 双 IH | 无探测，更糟 | 无探测 | ConflictProbe 硬失败 | **保持硬失败**；先卸 CR |
| 4 | Unload 短 drain | **10s** 先例 | 类似 | **5s** 默认 | 研究卸载可 10s |
| 5 | SSDT/hooklib 在树 | 不写 SSDT | 不写 SSDT | 写路径已冻结 | 保持 stub；勿开 fallback 上生产 |

### 6.1 明确 **不要** 从 IHPM 学的东西

1. 用 `MmGetSystemRoutineAddress` 做指针相等匹配。  
2. 单 magic `0x501802`。  
3. 无 ConflictProbe 的“detect 互相 re-init”。  
4. `initialize` 里绑死 CKCL 与解析（失败难 Cleanup）。  
5. README “latest” 替代支持矩阵。

### 6.2 明确 **可以** 参考的东西

1. **GetCpuClock 偏移表**（0x18 / 0x28 / 22000 分界）—— 已是族谱共识，TiDaoji 已有。  
2. **Unload 10s** 作为研究卸载安全默认值的上界参考。  
3. **KB 后双 Hvl pattern** 的工程习惯（失败硬返回）。  
4. Issue 清单：物理机、手动映射 NtTraceControl —— 写进 runbook。

### 6.3 明确 **优先** 对照 zhutingxf/CR 而非 IHPM 的东西

1. `0x601802`。  
2. 物理机 Halp / Hvlp 空指针。  
3. 22H2 / 22621 分段 pattern 注释。  
4. 睡眠–QPC 已知坏味道（文档化，不必立刻“修完美”）。

---

## 7. TiDaoji 当前引擎自检（相对族谱）

路径：`TiDaoji/infinity_hook/hook.cpp`（~887 行）> CR（~718）> IHPM（~445）。

| 检查项 | 状态 |
|--------|------|
| Port of CR + 生命周期修复 | ✓ NOTICE / README |
| 0x501802 + 0x601802 | ✓ |
| GetCpuClock 0x18@≥22000 | ✓ |
| Hvl 双 pattern | ✓ |
| Halp 物理机路径 | ✓ |
| ConflictProbe（禁 GetCpuClock==2 单独判冲突） | ✓ |
| Initialize 无 SYSTEMCALL；Start 安装 | ✓ |
| Repair / LayerBIntact / Cleanup | ✓ |
| 生产 SSDT 写冻结 | ✓（残余风险缓解） |
| Unload drain 可配置 | ✓ `TIDAOJI_UNLOAD_DRAIN_MS` |
| 支持矩阵（多 build 实测表） | 模板 + §8.1 **PENDING-ENV**（未伪造） |
| 24H2 本机验证记录 | ✗ **实验室待做**（env-blocked） |
| 门禁 `tools/verify_research_landed.sh` | ✓ |

**结论**：引擎侧不缺“再合一个 2023 ProMax”；in-repo 文档/代码缓解已落地；**目标机矩阵**仍待实验室填格。

---

## 8. 建议支持矩阵（填空模板）

复制到实验室 runbook；每格：`OK` / `Init-fail:reason` / `Start-fail:reason` / `BSOD:bugcheck` / `Hook-silent` / `PENDING-ENV`。

### 8.1 Live 执行状态（诚实）

| 项 | 状态 |
|----|------|
| 本机 (macOS 开发仓) | **无** Win11 VM / win-master 自动矩阵 runner |
| Live 22000 / 24H2 冒烟 | **env-blocked** — 未伪造通过格 |
| 模板 | 下表保留；实验室填格后更新本小节日期 |

| Build | 环境 | Initialize | Start | Hide 冒烟 | 2h 无 0x109 | 备注 |
|-------|------|------------|-------|-----------|-------------|------|
| 19041/19044 | VM | PENDING-ENV | PENDING-ENV | PENDING-ENV | PENDING-ENV | |
| 19044 + 近 CU | VM | PENDING-ENV | PENDING-ENV | PENDING-ENV | PENDING-ENV | Hvl 第二 pattern |
| 22000 | VM | PENDING-ENV | PENDING-ENV | PENDING-ENV | PENDING-ENV | 0x18 槽；**优先** |
| 22621 | VM | PENDING-ENV | PENDING-ENV | PENDING-ENV | PENDING-ENV | type 7 |
| 22631 | VM | PENDING-ENV | PENDING-ENV | PENDING-ENV | PENDING-ENV | |
| 23606+ | VM | PENDING-ENV | PENDING-ENV | PENDING-ENV | PENDING-ENV | **601802** |
| 26100 24H2 | VM | PENDING-ENV | PENDING-ENV | PENDING-ENV | PENDING-ENV | 优先；机制存疑直至实测 |
| 同 build | 物理机 | PENDING-ENV | PENDING-ENV | PENDING-ENV | PENDING-ENV | Halp type=5 路径 |
| 任意 | +CR 同机 | — | 期望 ConflictProbe | — | — | 负例；**先卸 CR** |

日志关键字：`Start: FAIL reason=ConflictProbe|EnableCkcl|InstallClocks|…`，`Init FAIL build=… symbol=…`。

---

## 9. 可选工程动作（按优先级）

| P | 动作 | 理由 | 工作量 |
|---|------|------|--------|
| — | 本文 + 残余缓解 + 门禁 | in-repo | **已落地**（PR3 tip） |
| **P0 产品** | **PR4** 插件/GUI 打磨 | 用户路径 | **下一目标**（先于实机矩阵） |
| **P0 运维** | **PR5** DSU 画像 A runbook | 装载可照做 | **PR4 后或并行** |
| P1 代码 | drain 5s / Init symbol 日志 / SSDT 冻结 | 残余风险 | **已落地** |
| **最后** | 目标 VM 支持矩阵 + VMP 冒烟 | 唯一兼容证据 | **不挡 PR4/5**；§8 PENDING-ENV |
| 之后 | zhutingxf pattern diff / PR6 硬化 | 矩阵失败再驱动 | P2 |
| **不做** | 整仓替换为 IHPM | 倒退 | — |
| **不做** | 开 `TIDAOJI_ALLOW_SSDT_FALLBACK` 上生产 | 经典 0x109 | — |

---

## 10. 外部补充区

### 10.1 原始研究（Grok / 本会话）

- 完成：§1–§9，基于 gh 源码与本地 CR/TiDaoji 对照。  
- 日期：2026-08-06。  
- 置信：族谱与源码对照 **高**；24H2 真实可挂性 **中**（依赖 zhutingxf 截图与公开 ETW 分析，缺本机矩阵）。

### 10.2 Claude 补充（2026-08-06，ask-models / Claude CLI）

> 独立上下文审稿。下列正文在保留 Claude 原意的前提下做了**轻度编辑**（去重复、对齐本文 § 编号）。  
> **[主编注]** 标记处：Claude 部分误读了本文（例如假定存在 “PG-safe” 断言、把 0x28 说成绝对结构尺寸、把 TiDaoji hide 当成 minifilter/驱动隐藏产品）。以 §1–§9 与设计文档为准。

#### 共识

1. **族谱与 IHPM 定位**：ProMax = 2023 冻结的 FiYHer 整理仓；“Win11:latest” 证据不足；**不要整仓替换进 TiDaoji** —— 同意。
2. **现代路径**：`>18363` 走 selector=2 + `HvlGetQpcBias`（及下游 Halp），经典任意函数指针塞 `GetCpuClock` 已死 —— 同意。
3. **双 magic（501802/601802）与物理机 Halp**：TiDaoji/CR/zhutingxf 强于 IHPM —— 同意；禁止回退单 magic。
4. **ConflictProbe 硬失败** 优于 IHPM 无探测 detect 互抢 —— 同意。
5. **任何变体不得称 PG-safe**；层 B 长驻风险独立存在 —— 同意（与本文 K20 / 残余风险 #1 一致）。
6. **支持矩阵实测** 是唯一真兼容证据；截图/README 只能当线索 —— 同意。

#### 分歧 / 纠错

| Claude 说法 | 主编裁决 |
|-------------|----------|
| 暗示正文曾写 “GetCpuClock 不在 PG 标准列表 / PG-safe” | **误读**。§1.3 / §3.6 / 设计 K20 已禁止该结论。保留其警告：**PG 巡检范围会扩**，历史“未监控”≠现在未监控。 |
| “§5.1 偏移 0x28 像绝对结构尺寸、全版本统一可疑” | **半对**。文中 0x18/0x28 是 **CKCL `WMI_LOGGER_CONTEXT` 内 GetCpuClock 槽相对 logger 基址的字段偏移**（族谱共识），不是整个结构 sizeof。22000 起改回 0x18 已有多源验证；仍应用目标 build 实测，但不必改成“绝对 0x28x”。 |
| 把 HalPrivateDispatch / KPTI trampoline 当成本文 § 中的 “IH 变体混写” | **误读对象**。本文未把 HAL dispatch 表 hook 算进 TiDaoji 引擎。可作为**平行技术**附录，与 IH 族解耦。 |
| HVCI vs KCFG 职责 | **有用补遗**。HVCI 主要卡**未签名装载/代码页**；间接调用校验更贴 **Kernel CFG**。指针改写在**可写数据**上时，运行时拦截点与装载门槛要分开写。 |
| 中期应迁 minifilter + ObRegisterCallbacks，弱化 syscall hook | **产品错位**。TiDaoji 目标是 **x64dbg + VMP 内核路径 Nt\*** hide（DebugPort/DebugObject 等），minifilter **替代不了** 这些语义。可作为**其它防护产品**路线，不写入 TiDaoji P0。 |
| 多核安装需 `KeIpiGenericCall` | **可选工程加强**。族谱主流实现多数未做；高核数上有理论窗口。列为 P2 研究，非当前阻塞（与固定 drain 同类：降概率非形式化）。 |

#### 补遗（采纳进知识库）

1. **CKCL session 生命周期**  
   - 部分环境 CKCL 非默认活跃；`StartTrace`/`NtTraceControl` 自行拉起是**可观测事件**。  
   - TiDaoji：`Initialize` 已有 EnsureCkclSession；失败应硬失败并打 reason（已部分具备）。

2. **EtwTi（Threat Intelligence）检测面**  
   - EDR/AC 可订阅相关 provider，间接看到 logger/session 异常。  
   - 研究用途可接受；若目标环境有强 EDR，属**检测风险**而非蓝屏风险，README 可一句带过。

3. **VM vs 物理机**  
   - Enlightened 定时器、PG 时序、嵌套虚拟化强制 HVCI 会导致行为差。  
   - 矩阵必须 **分列 VM / 物理机**（§8 模板已分；执行时勿合并单元格）。

4. **24H2（26100）不确定性层级**  
   - Claude 强调：不仅是“偏移待填”，而是 **ETW 热路径 / 字段是否仍可达** 的机制问题。  
   - 与 §3.6 / §9 P0 一致：**未逆向 + 未冒烟前，状态 = 未知**，不是“抄 zhutingxf 截图即 OK”。

5. **睡眠 / QPC**  
   - zhutingxf 已承认长睡后时间错；Claude 未否定。运维限制保留。

6. **平行 hook 族（非 IH）**  
   - `HalPrivateDispatchTable`、其它 ETW 发射点等可作为 **pattern 全挂时的 B 计划调研**，不与当前层 B 混为一谈，也**不**自动更 PG-safe。

#### 残余风险评议（对本文 §6 五条）

| # | 风险 | Claude | 主编 |
|---|------|--------|------|
| 1 | 层 B / NOT PG-safe | 同意；并强调 PG 可能已扩检 CKCL 指针 | **维持最高**；不降级 |
| 2 | pattern / 新 build | 同意；升级措辞为“结构存在性” | **维持**；矩阵 + 硬失败 |
| 3 | 双 IH ConflictProbe | （审稿未反对硬失败） | **维持硬失败** |
| 4 | Unload drain | 未反对加长；另提 IPI | drain **P1 可调 5–10s**；IPI 为 P2 |
| 5 | SSDT 写回归 | 同意冻结 | **维持 stub** |

**额外风险（写入清单，设计表可后续加行）**：

6. **CKCL/ETW session 可观测性**（检测，非必须 BSOD）。  
7. **高核数无 IPI 的安装窗口**（理论 BSOD）。  
8. **HVCI/测试签名/装载策略** 与 hook 运行时分离（画像 A 只动 DSE，不关 HVCI）。  
9. **长睡 QPC/时间漂移**（zhutingxf）。

#### 建议动作（与 §9 合并后的可执行列表）

**P0**

1. 支持矩阵：至少 **VM 22000 + VM 一档 24H2(26100)**；记录 `Initialize/Start` reason。  
2. 24H2：pattern 失败则对照 zhutingxf/CR 符号注释补特征；仍失败 → **机制存疑**，停扩。  
3. 文档与 README 继续禁止 PG-safe 措辞（已满足，保持）。

**P1**

4. `TIDAOJI_UNLOAD_DRAIN_MS` 研究默认 **5000–10000**（对齐 IHPM 经验）。  
5. Start/Init 失败日志保证带 **build + 符号名**。  
6. Runbook：与 CR 互斥；物理机单独矩阵格。

**P2**

7. 评估 InstallClocks 是否加 IPI 静默窗口（有实测再合）。  
8. 平行方案调研（非替换）：其它 ETW 热路径 / HAL 表 —— 仅当 IH 路径在目标 build **确认死亡**。

**明确不做**

- 整仓 IHPM。  
- 生产开 SSDT fallback。  
- 用 minifilter 替代 Nt* hide（产品不符）。

#### 置信与局限（Claude + 主编）

| 区域 | 置信 |
|------|------|
| IHPM 冻结、族谱、TiDaoji>IHPM、双 magic/物理路径存在 | **高** |
| HVCI≠KCFG、CKCL 可观测、24H2 需自测 | **高** |
| PG 是否已巡检 GetCpuClock/Hvl（具体 build） | **中低**（需实测/逆向，禁止脑补） |
| 24H2 可挂性 | **未知** 直至矩阵 |
| IPI 必要性 | **中**（理论有窗口，族谱少做） |

### 10.3 修订历史

| 版本 | 日期 | 说明 |
|------|------|------|
| Rev 0 | 2026-08-06 | 初稿：IHPM 全量分析 + 族谱 + 残余风险对照 + 矩阵模板 |
| Rev 1 | 2026-08-06 | 并入 Claude 审稿 §10.2；主编注纠正误读与产品错位 |
| Rev 2 | 2026-08-07 | **In-repo 落地**：drain 5s；Init symbol 日志；README 运维限制；§8 PENDING-ENV；`tools/verify_research_landed.sh` |

### 10.4 In-repo 落地清单（相对验收）

| 项 | 位置 |
|----|------|
| 研究全文 + 硬结论不 vendor IHPM | 本文 §1–§10 |
| NOT PG-safe | README 顶栏 + 本文 + 设计 K20 |
| 交叉链接 | README、设计 Rev 6、本文关联设计 |
| SSDT/hooklib 写路径冻结 | `ssdt.cpp` / `hooklib.cpp` `#ifndef TIDAOJI_ALLOW_SSDT_FALLBACK` |
| Unload drain ≥2s 且研究对齐 5s | `TiDaoji.cpp` `TIDAOJI_UNLOAD_DRAIN_MS` 默认 5000 |
| Start FAIL reason 分流 | `infinity_hook/hook.cpp` |
| Init FAIL build+symbol | `ResolvePatterns` |
| 生产 hide 仅 GetFunctionAddress + k_hook | `hooks.cpp` |
| Live 矩阵 | §8.1 env-blocked |
| 门禁 | `tools/verify_research_landed.sh` |

---

## 附录 A — 关键 URL

| 资源 | URL |
|------|-----|
| InfinityHookProMax | https://github.com/ThomasonZhao/InfinityHookProMax |
| FiYHer InfinityHookPro | https://github.com/FiYHer/InfinityHookPro |
| zhutingxf InfinityHookPro | https://github.com/zhutingxf/InfinityHookPro |
| everdox InfinityHook | https://github.com/everdox/InfinityHook |
| FiYHer issue #17 (KB pattern) | https://github.com/FiYHer/InfinityHookPro/issues/17 |
| IHPM physical | https://github.com/ThomasonZhao/InfinityHookProMax/issues/1 |
| freebuf 2004 路径 | https://www.freebuf.com/articles/system/278857.html |
| 本地 CR | `/Users/daoji/Code/CR_Full/驱动源码/SakDriver/infinity_hook_pro/` |
| TiDaoji 引擎 | `/Users/daoji/Code/TitanHide/TiDaoji/infinity_hook/` |
| TiDaoji 设计 | `docs/2026-08-06-tidaoji-infinityhook-design.md` |

## 附录 B — IHPM `start()` 语义伪代码

```text
if !callback or !valid(GetCpuClock_ptr): fail
if build <= 18363:
  *GetCpuClock = self_get_cpu_clock
else:
  original_GetCpuClock = *GetCpuClock
  *GetCpuClock = 2
  original_Hvl = *HvlGetQpcBias_ptr
  *HvlGetQpcBias_ptr = self_hvl_get_qpc_bias
once: PsCreateSystemThread(detect_routine)  // only re-fix for <=18363
```

## 附录 C — TiDaoji `Start()` 语义伪代码（对照）

```text
lock life
if !Ready: fail
if IsStarted:
  if LayerBIntact: ok else Repair
if ConflictProbe: fail reason=ConflictProbe   // 双 IH
EnableCkclSyscall or fail
InstallClocks:   // 含物理机 Halp / Hvlp fake / FakeHvl
  <=18363: pointer
  else: selector 2 + FakeHvl + optional physical
EnsureDetectThread  // Repair 现代层 B
IsStarted = true
unlock
```

---

**文档结束（Rev 2 landed）**
