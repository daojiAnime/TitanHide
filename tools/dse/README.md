# DSE helpers (profile A — temporary only)

> **Research / lab only.** Temporary DSE weaken for loading **unsigned** `TiDaoji.sys`.  
> **Does not** disable PatchGuard. Restore DSE immediately after `sc start`.

## Layout

| Path | Role |
|------|------|
| `kdu/kdu.exe` | Kernel Driver Utility (hfiref0x lineage) — ` -dse <value>` |
| `kdu/drv64.dll` | KDU provider database (required next to `kdu.exe`) |
| `DisabledDSE.exe` | Alternate small helper (optional path) |
| `dse_off.bat` | Write DSE flags → `0` (allow unsigned load) |
| `dse_on.bat` | Restore DSE flags → `6` (default on win-master 19045) |
| `load_tidaoji_profile_a.bat` | Full A sequence: off → install/start → on → smoke optional |

## Verified hashes (win-master 2026-08-07)

```
SHA256(kdu.exe)     = a1e313901096e033eaa377639b3430e017b166cd775c47990c2ce09f4066a572
SHA256(drv64.dll)   = e08db83d3720947b0749730d7d744b0b6cd0b57d9d759a3ee2899491d2397ce2
SHA256(DisabledDSE) = a8cec0e548cda00527bff4f5ded36c5892a6f664150cc82444402e39b967a0fd
```

## Usage (elevated admin)

```bat
cd tools\dse
dse_off.bat
..\..\install_driver.bat
dse_on.bat
```

Or one-shot (expects `TiDaoji.sys` already built):

```bat
tools\dse\load_tidaoji_profile_a.bat
```

`DSE_ON_VALUE` default **6** (observed stock `g_CiOptions` on Win10 19045). Override:

```bat
set DSE_ON_VALUE=6
dse_on.bat
```

## Upstream

- KDU: https://github.com/hfiref0x/KDU (MIT-style third-party; see their license)  
- Binaries vendored for **reproducible lab** — update with care; re-hash after replace.

## Legal / safety

- Uses vulnerable-driver / CI flag write techniques. **Never** on production or internet-facing hosts.  
- TiDaoji remains **NOT PG-safe** after load.
