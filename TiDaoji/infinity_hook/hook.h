#pragma once
#include <ntifs.h>

// InfinityHook Pro port for TiDaoji (from CR infinity_hook_pro).
// Lifecycle contracts: K13/K17/K18/K19 — see docs/2026-08-06-tidaoji-infinityhook-design.md

typedef void(__fastcall* InfinityCallbackPtr)(unsigned long nCallIndex, PVOID* pCallAddress);

namespace k_hook
{
	// Ready only: resolve patterns + stock snapshots. No SYSTEMCALL, no clock rewrite.
	bool Initialize(InfinityCallbackPtr ssdtCallBack);

	// Running: ConflictProbe → CKCL SYSTEMCALL → install clocks → DetectThread.
	// Idempotent when LayerBIntact; else Repair when already started.
	bool Start();

	// Full teardown of layer B. Requires IsStarted; clean when already stopped.
	bool Stop();

	// Strategy A: reinstall clocks if layer B stripped; DetectThread calls only this.
	bool Repair();

	// Unconditional cleanup (Running → Stop; partial Start → Rollback). Does not depend on IsStarted.
	bool Cleanup();

	// True when layer B matches our install for current build.
	bool LayerBIntact();

	// True if another InfinityHook-like agent appears to own layer B.
	bool ConflictProbe();

	// Running state
	bool IsRunning();
}
