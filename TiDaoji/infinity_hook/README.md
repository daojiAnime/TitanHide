# TiDaoji InfinityHook (PR2 engine / PR3 wired)

Port of CR `infinity_hook_pro` with lifecycle fixes (design K13/K17/K18/K19).

## API

| Function | Role |
|----------|------|
| `Initialize(cb)` | Ready: patterns + stock; **no** SYSTEMCALL, **no** clock rewrite |
| `Start()` | ConflictProbe → CKCL SYSTEMCALL → InstallClocks → DetectThread; sets `IsStarted` |
| `Repair()` | Reinstall clocks if layer B stripped (DetectThread only calls this) |
| `Stop()` | Full teardown when running |
| `Cleanup()` | Unconditional (partial or full) |
| `LayerBIntact()` / `ConflictProbe()` | Probes |

## PR2 vs PR3

- **PR2**: engine compiles into `TiDaoji.sys`; production hide still SSDT.
- **PR3**: `Hooks::Initialize` → resolve SSDT originals → `k_hook::Initialize(TiDaojiSyscallCallback)` → `Start()`.  
  Callback: pointer equality vs saved originals → swap to `HookNt*`. No SSDT table write.

## Pool tag

`'Tdji'`
