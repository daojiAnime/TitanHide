# TiDaoji InfinityHook (PR2)

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

- **PR2**: engine compiles into `TiDaoji.sys`; production hide still **SSDT**.
- **PR3**: `Hooks::Initialize` switches to `k_hook::Start` + pointer-swap callback.

Optional self-test: compile with `TIDAOJI_IH_SELFTEST` (DriverEntry Start→Stop).

## Pool tag

`'Tdji'`
