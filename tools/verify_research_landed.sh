#!/usr/bin/env bash
# verify_research_landed.sh — structural gate for research-doc in-repo landing.
# Drives real repo files (docs + shipped TiDaoji sources). Exit 0 only if all checks pass.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
fail=0
pass() { echo "PASS: $*"; }
bad()  { echo "FAIL: $*"; fail=1; }

echo "=== verify_research_landed @ $ROOT ==="

# --- 1. Research document sections ---
R="docs/2026-08-06-infinityhook-lineage-newos-research.md"
[[ -f "$R" ]] || bad "missing $R"
for needle in \
  "族谱位置" \
  "InfinityHookProMax" \
  "NOT PG-safe" \
  "建议支持矩阵" \
  "PENDING-ENV" \
  "10.2 Claude" \
  "整仓替换" \
  "不建议把 ProMax 整仓 vendor" \
  "TIDAOJI_UNLOAD_DRAIN_MS" \
  "ConflictProbe"
do
  if grep -qF "$needle" "$R"; then
    pass "research contains: $needle"
  else
    bad "research missing: $needle"
  fi
done

# Must not claim PG-safe as positive
if grep -Eiq 'is PG-safe|PG-safe\s*[=:]|声称.*PG-safe|证明 PG-safe' "$R"; then
  # allow "NOT PG-safe" / "不得称 PG-safe" / "不声称 PG-safe"
  if grep -Eiq '(is PG-safe|PG-safe\s*=\s*true|已验证 PG)' "$R"; then
    bad "research appears to claim PG-safe"
  else
    pass "research does not claim PG-safe (negation language only)"
  fi
else
  pass "research PG-safe language scan clean"
fi

# Hard: no wholesale vendor recommendation as todo
if grep -qF "整仓替换为 IHPM" "$R" && grep -qF "不做" "$R"; then
  pass "research forbids wholesale IHPM replace"
else
  bad "research missing explicit non-goal: no IHPM replace"
fi

# --- 2. Cross-links README + design ---
for f in README.md docs/2026-08-06-tidaoji-infinityhook-design.md; do
  if grep -qF "2026-08-06-infinityhook-lineage-newos-research.md" "$f"; then
    pass "crosslink in $f"
  else
    bad "missing research crosslink in $f"
  fi
done
if grep -qF "NOT PG-SAFE" README.md || grep -qF "NOT PG-safe" README.md; then
  pass "README residual NOT PG-SAFE"
else
  bad "README missing NOT PG-SAFE"
fi
if grep -Eiq 'is PG-safe|PG-safe\s*=\s*true' README.md; then
  bad "README claims PG-safe"
else
  pass "README does not claim PG-safe"
fi

# --- 3. Code mitigations ---
if grep -qF "TIDAOJI_ALLOW_SSDT_FALLBACK" TiDaoji/ssdt.cpp \
  && grep -qF "#ifndef TIDAOJI_ALLOW_SSDT_FALLBACK" TiDaoji/ssdt.cpp \
  && grep -qF "SSDT::Hook frozen" TiDaoji/ssdt.cpp; then
  pass "SSDT write path frozen by default"
else
  bad "SSDT freeze guard missing"
fi
if grep -qF "#ifndef TIDAOJI_ALLOW_SSDT_FALLBACK" TiDaoji/hooklib.cpp \
  && grep -qF "Hooklib::Hook frozen" TiDaoji/hooklib.cpp; then
  pass "hooklib write path frozen by default"
else
  bad "hooklib freeze guard missing"
fi

DRAIN_LINE="$(grep -E 'define TIDAOJI_UNLOAD_DRAIN_MS' TiDaoji/TiDaoji.cpp || true)"
if [[ -z "$DRAIN_LINE" ]]; then
  bad "TIDAOJI_UNLOAD_DRAIN_MS missing"
else
  MS="$(echo "$DRAIN_LINE" | grep -oE '[0-9]+' | tail -1)"
  if [[ "$MS" -ge 2000 ]]; then
    pass "Unload drain default ${MS}ms (>=2000)"
  else
    bad "Unload drain ${MS}ms < 2000"
  fi
fi

if grep -qF 'Start: FAIL reason=ConflictProbe' TiDaoji/infinity_hook/hook.cpp \
  && grep -qF 'Start: FAIL reason=InstallClocks' TiDaoji/infinity_hook/hook.cpp \
  && grep -qF 'Start: FAIL reason=EnableCkclSyscall' TiDaoji/infinity_hook/hook.cpp; then
  pass "Start FAIL reason= strings present"
else
  bad "Start FAIL reason= strings incomplete"
fi

if grep -qF 'Init FAIL build=' TiDaoji/infinity_hook/hook.cpp \
  && grep -qF 'symbol=' TiDaoji/infinity_hook/hook.cpp; then
  pass "Init FAIL build+symbol logs present"
else
  bad "Init FAIL build+symbol logs missing"
fi

# --- 4. Production hide uses GetFunctionAddress + k_hook, not SSDT::Hook ---
if grep -qF 'SSDT::GetFunctionAddress' TiDaoji/hooks.cpp \
  && grep -qF 'k_hook::Start' TiDaoji/hooks.cpp \
  && grep -qF 'k_hook::Initialize' TiDaoji/hooks.cpp; then
  pass "hooks.cpp uses GetFunctionAddress + k_hook"
else
  bad "hooks.cpp missing IH production path"
fi
# Production Initialize must not call SSDT::Hook(
if grep -n 'SSDT::Hook(' TiDaoji/hooks.cpp; then
  bad "hooks.cpp still calls SSDT::Hook (production write)"
else
  pass "hooks.cpp does not call SSDT::Hook"
fi
if grep -n 'SSDT::Hook(' TiDaoji/TiDaoji.cpp; then
  bad "TiDaoji.cpp calls SSDT::Hook"
else
  pass "TiDaoji.cpp does not call SSDT::Hook"
fi

# --- 5. Magic dual + no single-magic regression marker in engine ---
if grep -qF 'INFINITYHOOK_MAGIC_501802' TiDaoji/infinity_hook/hook.cpp \
  && grep -qF 'INFINITYHOOK_MAGIC_601802' TiDaoji/infinity_hook/hook.cpp; then
  pass "dual stack magic 501802+601802"
else
  bad "missing dual stack magic"
fi

# --- 6. PR4/PR5 userland + runbook (non-live) ---
if grep -qF 'TiDaojiHelp' TiDaoji_x64dbg/plugin.cpp \
  && grep -qF 'TiDaojiStatus' TiDaoji_x64dbg/plugin.cpp \
  && grep -qF 'TiDaojiUnhideAll' TiDaoji_x64dbg/plugin.cpp \
  && grep -qF 'PLUGIN_VERSION 2' TiDaoji_x64dbg/plugin.h; then
  pass "PR4 plugin commands + v2"
else
  bad "PR4 plugin polish missing"
fi
if grep -qF 'WritePrivateProfileStringA("TiDaoji", "Type"' TiDaojiGUI/main.cpp \
  && grep -qF 'GetPrivateProfileStringA("TiDaoji", "Type"' TiDaojiGUI/main.cpp; then
  pass "PR4 GUI Type persistence"
else
  bad "PR4 GUI Type persistence missing"
fi
RB="docs/2026-08-07-tidaoji-dsu-profile-a-runbook.md"
if [[ -f "$RB" ]] && grep -qF '画像 A' "$RB" && grep -qF 'ConflictProbe' "$RB" \
  && grep -qF 'restore' "$RB" && grep -qF 'NOT PG-safe' "$RB"; then
  pass "PR5 DSU profile A runbook"
else
  bad "PR5 runbook missing or incomplete"
fi
if grep -qF '2026-08-07-tidaoji-dsu-profile-a-runbook' README.md \
  && grep -qF 'TiDaojiHelp' README.md; then
  pass "README links PR4/PR5"
else
  bad "README missing PR4/PR5 links"
fi

# --- 7. Olly / TitanEngine + shared user_client ---
if [[ -f TiDaoji/user_client.h ]] \
  && grep -qF 'TiDaojiUserCall' TiDaoji/user_client.h \
  && grep -qF 'TIDAOJI_DEFAULT_TYPE' TiDaoji/user_client.h; then
  pass "user_client.h present"
else
  bad "user_client.h missing"
fi
if grep -qF 'user_client.h' TiDaoji_OllyDbg/TiDaojiOlly.cpp \
  && grep -qF 'TiDaojiUserCall' TiDaoji_OllyDbg/TiDaojiOlly.cpp \
  && grep -qF '"TiDaojiOlly"' TiDaoji_OllyDbg/TiDaojiOlly.cpp; then
  pass "Olly uses user_client + ini basename"
else
  bad "Olly polish incomplete"
fi
if grep -qF 'user_client.h' TiDaoji_TitanEngine/TiDaojiTE.cpp \
  && grep -qF 'TiDaojiUserCall' TiDaoji_TitanEngine/TiDaojiTE.cpp \
  && grep -qF '*titanPluginMajorVersion = 2' TiDaoji_TitanEngine/TiDaojiTE.cpp; then
  pass "TitanEngine uses user_client v2"
else
  bad "TitanEngine polish incomplete"
fi

# --- 8. Vendored DSE lab tools (profile A) ---
if [[ -f tools/dse/kdu/kdu.exe && -f tools/dse/kdu/drv64.dll \
  && -f tools/dse/dse_off.bat && -f tools/dse/dse_on.bat \
  && -f tools/dse/load_tidaoji_profile_a.bat && -f tools/dse/NOTICE ]]; then
  pass "tools/dse KDU + scripts tracked"
else
  bad "tools/dse incomplete"
fi
if grep -qF 'tools/dse' docs/2026-08-07-tidaoji-dsu-profile-a-runbook.md \
  && grep -qF 'tools/dse' README.md; then
  pass "docs/README point at tools/dse"
else
  bad "docs missing tools/dse links"
fi

# --- 9. L2/L3 loader profiles ---
if grep -qF 'IoCreateDriver' TiDaoji/TiDaoji.cpp \
  && grep -qF 'SoftUnload' TiDaoji/TiDaoji.h \
  && grep -qF 'SoftUnload' TiDaoji/TiDaoji.cpp; then
  pass "manual-map Entry + SoftUnload"
else
  bad "L2/L3 driver contract missing"
fi
if [[ -f tools/loader/L2_kdmapper.bat && -f tools/loader/L3_multi_provider.bat \
  && -f tools/loader/load_auto.bat && -f tools/loader/soft_unload.bat \
  && -f docs/2026-08-07-tidaoji-loader-profiles-L1-L3.md ]]; then
  pass "L2/L3 loader scripts + doc"
else
  bad "loader toolkit incomplete"
fi
if grep -qF 'TiDaojiSoftUnload' TiDaoji_x64dbg/plugin.cpp; then
  pass "plugin SoftUnload command"
else
  bad "plugin SoftUnload missing"
fi

# --- 10. Adversarial / red-team desk completeness (goal: 10/10 docs) ---
ADV="docs/2026-08-07-tidaoji-loader-profiles-L1-L3.md"
for needle in \
  "对抗 / 红队完备" \
  "评分量表" \
  "desk research" \
  "L1 检测矩阵" \
  "L2 / L3 检测矩阵" \
  "PiDDBCacheTable" \
  "MmUnloadedDrivers" \
  "g_KernelHashBucketList" \
  "WdFilter" \
  "TiDaoji 专用暴露面" \
  "SoftUnload 之后还剩什么" \
  "kill-switch" \
  "明确不缓解" \
  "0xC0000603" \
  "HVCI" \
  "CKCL" \
  "KB5020779"
do
  if grep -qF "$needle" "$ADV"; then
    pass "adversarial doc: $needle"
  else
    bad "adversarial doc missing: $needle"
  fi
done
# device path appears as \\\\.\\TiDaoji or \\.\TiDaoji in markdown
if grep -qE '\\\\\.\\TiDaoji|\\\\\.\\\\TiDaoji|Device\\\\TiDaoji' "$ADV" \
  || grep -qF 'TiDaoji' "$ADV" && grep -qF '设备' "$ADV"; then
  pass "adversarial doc: device/TiDaoji exposure"
else
  bad "adversarial doc missing device exposure"
fi
if grep -qF '非 stealth' "$ADV" || grep -qF '≠ live stealth' "$ADV" || grep -qF '不是 10/10' "$ADV"; then
  pass "adversarial rubric bounds honest"
else
  bad "adversarial rubric missing honesty bound"
fi

echo "=== summary fail=$fail ==="
exit "$fail"
