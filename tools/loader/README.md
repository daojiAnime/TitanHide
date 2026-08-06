# TiDaoji optional loaders (L1 / L2 / L3)

**Full research note (no live L2/L3 AC lab):**  
`docs/2026-08-07-tidaoji-loader-profiles-L1-L3.md` (Rev 3)  
Adversarial desk-research chapter: **§12 对抗/红队完备** (rubric = documentation 10/10, not stealth).

| Script | Profile | Needs |
|--------|---------|--------|
| `../dse/load_tidaoji_profile_a.bat` | **L1** | vendored KDU (`tools/dse`) |
| `L2_kdmapper.bat` | **L2** | external `KDMAPPER` (e.g. TheCruZ/kdmapper build) |
| `L3_multi_provider.bat` | **L3** | `TIDAOJI_MAPPER` + optional `TIDAOJI_PROVIDER` |
| `load_auto.bat` | auto | L2 if `KDMAPPER` set, else L3 if `TIDAOJI_MAPPER`, else L1 |
| `soft_unload.bat` | all | device + SoftUnload |
| `providers.example.ini` | L3 | template only |

## Environment variables

| Var | Meaning |
|-----|---------|
| `TIDAOJI_SYS` | Path to `TiDaoji.sys` |
| `KDMAPPER` | Full path to kdmapper-compatible CLI |
| `TIDAOJI_MAPPER` | L3 custom mapper executable |
| `TIDAOJI_MAPPER_ARGS` | Extra args before sys path |
| `TIDAOJI_PROVIDER` | Provider id for multi-provider mappers (`-prv`) |

## Preflight (L2/L3, from public kdmapper docs)

Before map (lab checklist, not automated):

1. **HVCI / Memory integrity** often must be off for classic iqvw path.  
2. **Vulnerable Driver Blocklist**: if mapper returns `0xC0000603`, see KB5020779 / `VulnerableDriverBlocklistEnable`.  
3. No leftover **`\\Device\\Nal`** (prior iqvw/kdmapper crash).  
4. Do **not** pass mapper **`--free`** for long-lived TiDaoji (would free the image).  
5. After work: `soft_unload.bat`; expect **reboot** for full cleanliness of mapped pages.

## Driver contract (in-tree)

| Item | Behavior |
|------|----------|
| `DriverObject == NULL` | `IoCreateDriver` then full init |
| Device | `\\.\TiDaoji` (default name) |
| SoftUnload | `HIDE_INFO.Command = SoftUnload` |
| Plugin | `TiDaojiSoftUnload` |

## Explicit non-goals

- No in-tree exploit / vulnerable-driver payload.  
- No guarantee of Win11 24H2+ iqvw success.  
- NOT PG-safe.

## Upstream pointer

- https://github.com/TheCruZ/kdmapper — build yourself; set `KDMAPPER` to the resulting exe.
