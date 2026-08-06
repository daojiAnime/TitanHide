# TiDaoji optional loaders (L1 / L2 / L3)

See also: `docs/2026-08-07-tidaoji-loader-profiles-L1-L3.md`

| Script | Profile | Needs |
|--------|---------|--------|
| `../dse/load_tidaoji_profile_a.bat` | **L1** | vendored KDU |
| `L2_kdmapper.bat` | **L2** | external `KDMAPPER` exe |
| `L3_multi_provider.bat` | **L3** | `TIDAOJI_MAPPER` + args |
| `load_auto.bat` | auto | tries L2 if KDMAPPER set, else L1 |
| `soft_unload.bat` | all | device open + SoftUnload |

## Environment variables

| Var | Meaning |
|-----|---------|
| `TIDAOJI_SYS` | Path to `TiDaoji.sys` (default: search build outs / `D:\src\TiDaoji.sys`) |
| `KDMAPPER` | Full path to kdmapper-compatible CLI |
| `TIDAOJI_MAPPER` | L3 custom mapper executable |
| `TIDAOJI_MAPPER_ARGS` | Extra args before sys path (optional) |
| `TIDAOJI_PROVIDER` | L3 provider id/name passed if mapper supports `-prv` |

## Driver contract (built-in)

- Manual map with `DriverObject==NULL` → `IoCreateDriver`
- Soft unload: `HIDE_INFO.Command = SoftUnload` or `soft_unload.bat`

**NOT PG-safe.** Lab only.
