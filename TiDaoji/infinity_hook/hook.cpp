#pragma warning(disable : 4201 4819 4311 4302 4996)
#include "hook.h"
#include "headers.h"
#include "defines.h"
#include "utils.hpp"

// TiDaoji InfinityHook lifecycle port (CR infinity_hook_pro + K13/K17/K18/K19)

namespace k_hook
{
	static InfinityCallbackPtr m_InfinityCallback = nullptr;
	static unsigned long m_BuildNumber = 0;
	static void* m_SystemCallTable = nullptr;
	static volatile bool m_DetectThreadStatus = false;
	static void* m_EtwpDebuggerData = nullptr;
	static void* m_CkclWmiLoggerContext = nullptr;
	static void** m_EtwpDebuggerDataSilo = nullptr;
	static void** m_GetCpuClock = nullptr;
	static PETHREAD m_DetectThreadObject = NULL;
	static PLONGLONG m_QpcPointer = NULL;
	static PMDL m_QpcMdl = NULL;

	// Stock / original values (saved before we mutate)
	static unsigned long long m_StockGetCpuClock = 0;
	static unsigned long long m_OriginalGetCpuClock = 0;
	static unsigned long long m_HvlpReferenceTscPage = 0;
	static unsigned long long m_HvlGetQpcBias = 0;
	static unsigned long long m_HvlpGetReferenceTimeUsingTscPage = 0;
	static unsigned long long m_HalpPerformanceCounter = 0;
	static unsigned long long m_HalpOriginalPerformanceCounter = 0;
	static unsigned long long m_HalpOriginalPerformanceCounterCopy = 0;
	static unsigned long* m_HalpPerformanceCounterType = 0;
	static unsigned char m_VmHalpPerformanceCounterType = 0;
	static unsigned long m_OriginalHalpPerformanceCounterType = 0;
	static unsigned long long m_OriginalHvlpGetReferenceTimeUsingTscPage = 0;
	typedef __int64 (*FHvlGetQpcBias)();
	static FHvlGetQpcBias m_OriginalHvlGetQpcBias = nullptr;
	static CLIENT_ID m_ClientId = { 0 };

	static bool IsStarted = false;
	static bool m_Ready = false;
	static bool m_CkclSyscallEnabled = false;
	static bool m_ClocksInstalled = false;
	static bool m_PhysicalMachineHalpPath = false;
	static bool m_HvlpFakeInstalled = false;
	static KGUARDED_MUTEX m_LifeMutex;
	static bool m_LifeMutexInit = false;

	static const ULONG kPoolTag = 'Tdji';

#define HALP_PERFORMANCE_COUNTER_TYPE_OFFSET (0xE4)
#define HALP_PERFORMANCE_COUNTER_BASE_RATE_OFFSET (0xC0)
#define HALP_PERFORMANCE_COUNTER_TYPE_PHYSICAL_MACHINE (0x5)
#define HALP_PERFORMANCE_COUNTER_BASE_RATE (10000000i64)

	static void EnsureLifeMutex()
	{
		if (!m_LifeMutexInit)
		{
			KeInitializeGuardedMutex(&m_LifeMutex);
			m_LifeMutexInit = true;
		}
	}

// MSVC and clang both accept ##__VA_ARGS__ swallowing trailing comma.
#define Log(fmt, ...) DbgPrintEx(0, 0, "[TIDAOJI][IH] " fmt "\n", ##__VA_ARGS__)

	// --- CKCL control ---
	// EtwpUpdateTrace with SYSTEMCALL flag enables syscall tracing.
	// EtwpStartTrace starts session; Stop+Start clears flags (CR Stop pattern).

	static NTSTATUS EventTraceControl(ETWP_TRACE_TYPE nType, bool enableSyscall)
	{
		CKCL_TRACE_PROPERTIES* pProperty =
			(CKCL_TRACE_PROPERTIES*)ExAllocatePoolWithTag(NonPagedPool, PAGE_SIZE, kPoolTag);
		if (!pProperty)
			return STATUS_MEMORY_NOT_ALLOCATED;

		wchar_t* szProviderName =
			(wchar_t*)ExAllocatePoolWithTag(NonPagedPool, 256 * sizeof(wchar_t), kPoolTag);
		if (!szProviderName)
		{
			ExFreePoolWithTag(pProperty, kPoolTag);
			return STATUS_MEMORY_NOT_ALLOCATED;
		}

		RtlZeroMemory(pProperty, PAGE_SIZE);
		RtlZeroMemory(szProviderName, 256 * sizeof(wchar_t));
		RtlCopyMemory(szProviderName, L"Circular Kernel Context Logger",
			sizeof(L"Circular Kernel Context Logger"));
		RtlInitUnicodeString(&pProperty->ProviderName, (const wchar_t*)szProviderName);

		GUID guidCkclSession = { 0x54dea73a, 0xed1f, 0x42a4,
			{ 0xaf, 0x71, 0x3e, 0x63, 0xd0, 0x56, 0xf1, 0x74 } };

		pProperty->Wnode.BufferSize = PAGE_SIZE;
		pProperty->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
		pProperty->Wnode.Guid = guidCkclSession;
		pProperty->Wnode.ClientContext = 3;
		pProperty->BufferSize = sizeof(unsigned long);
		pProperty->MinimumBuffers = 2;
		pProperty->MaximumBuffers = 2;
		pProperty->LogFileMode = EVENT_TRACE_BUFFERING_MODE;

		unsigned long nLength = 0;
		if (nType == ETWP_TRACE_TYPE::EtwpUpdateTrace && enableSyscall)
			pProperty->EnableFlags = EVENT_TRACE_FLAG_SYSTEMCALL;

		NTSTATUS ntStatus = NtTraceControl(nType, pProperty, PAGE_SIZE, pProperty, PAGE_SIZE, &nLength);

		ExFreePoolWithTag(szProviderName, kPoolTag);
		ExFreePoolWithTag(pProperty, kPoolTag);
		return ntStatus;
	}

	static bool EnsureCkclSessionRunning()
	{
		// Prefer update (session exists). If fail, start then update without SYSTEMCALL.
		if (NT_SUCCESS(EventTraceControl(EtwpUpdateTrace, false)))
			return true;
		if (!NT_SUCCESS(EventTraceControl(EtwpStartTrace, false)))
		{
			Log("start ckcl fail");
			return false;
		}
		if (!NT_SUCCESS(EventTraceControl(EtwpUpdateTrace, false)))
		{
			Log("ckcl update (no syscall) fail");
			return false;
		}
		return true;
	}

	static bool EnableCkclSyscall()
	{
		if (!NT_SUCCESS(EventTraceControl(EtwpUpdateTrace, true)))
		{
			// Session might not exist
			if (!NT_SUCCESS(EventTraceControl(EtwpStartTrace, false)))
			{
				Log("EnableCkclSyscall start fail");
				return false;
			}
			if (!NT_SUCCESS(EventTraceControl(EtwpUpdateTrace, true)))
			{
				Log("EnableCkclSyscall update fail");
				return false;
			}
		}
		m_CkclSyscallEnabled = true;
		Log("CKCL SYSTEMCALL enabled");
		return true;
	}

	static void DisableCkclSyscall()
	{
		// CR pattern: stop + start session to clear flags
		NTSTATUS st1 = EventTraceControl(EtwpStopTrace, false);
		NTSTATUS st2 = EventTraceControl(EtwpStartTrace, false);
		m_CkclSyscallEnabled = false;
		Log("CKCL SYSTEMCALL cleared stop=%08X start=%08X", st1, st2);
	}

	// --- GetCpuClock hook path ---

	static unsigned long long SelfGetCpuClock()
	{
		if (ExGetPreviousMode() == KernelMode)
			return __rdtsc();

		PKTHREAD pCurrentThread = (PKTHREAD)__readgsqword(0x188);
		unsigned int nCallIndex = 0;
		if (m_BuildNumber <= 7601)
			nCallIndex = *(unsigned int*)((unsigned long long)pCurrentThread + 0x1f8);
		else
			nCallIndex = *(unsigned int*)((unsigned long long)pCurrentThread + 0x80);

		void** pStackMax = (void**)__readgsqword(0x1a8);
		void** pStackFrame = (void**)_AddressOfReturnAddress();

		for (void** pStackCurrent = pStackMax; pStackCurrent > pStackFrame; --pStackCurrent)
		{
#define INFINITYHOOK_MAGIC_501802 ((unsigned long)0x501802)
#define INFINITYHOOK_MAGIC_601802 ((unsigned long)0x601802)
#define INFINITYHOOK_MAGIC_F33 ((unsigned short)0xF33)

			unsigned long* pValue1 = (unsigned long*)pStackCurrent;
			if ((*pValue1 != INFINITYHOOK_MAGIC_501802) &&
				(*pValue1 != INFINITYHOOK_MAGIC_601802))
				continue;

			--pStackCurrent;
			unsigned short* pValue2 = (unsigned short*)pStackCurrent;
			if (*pValue2 != INFINITYHOOK_MAGIC_F33)
				continue;

			for (; pStackCurrent < pStackMax; ++pStackCurrent)
			{
				unsigned long long* pllValue = (unsigned long long*)pStackCurrent;
				if (!(PAGE_ALIGN(*pllValue) >= m_SystemCallTable &&
					PAGE_ALIGN(*pllValue) < (void*)((unsigned long long)m_SystemCallTable + (PAGE_SIZE * 2))))
					continue;

				void** pSystemCallFunction = &pStackCurrent[9];
				if (m_InfinityCallback)
					m_InfinityCallback(nCallIndex, pSystemCallFunction);
				break;
			}
			break;
		}
		return __rdtsc();
	}

	EXTERN_C static __int64 FakeHvlGetQpcBias()
	{
		SelfGetCpuClock();
		if (*((unsigned long long*)m_HvlpReferenceTscPage) != 0)
			return *((unsigned long long*)(*((unsigned long long*)m_HvlpReferenceTscPage)) + 3);
		return 0;
	}

	static ULONG64 FakeGetReferenceTimeUsingTscPage()
	{
		return __rdtsc();
	}

	// --- Layer B probes ---

	bool LayerBIntact()
	{
		if (!m_GetCpuClock || !MmIsAddressValid(m_GetCpuClock))
			return false;

		if (m_BuildNumber <= 18363)
		{
			return MmIsAddressValid(*m_GetCpuClock) &&
				*m_GetCpuClock == (void*)SelfGetCpuClock;
		}

		// Modern path: selector 2 + our Hvl fake
		if ((unsigned long long)(*m_GetCpuClock) != 2)
			return false;
		if (!m_HvlGetQpcBias || !MmIsAddressValid((void*)m_HvlGetQpcBias))
			return false;
		return *((unsigned long long*)m_HvlGetQpcBias) == (unsigned long long)FakeHvlGetQpcBias;
	}

	bool ConflictProbe()
	{
		// Must not treat GetCpuClock==2 alone as conflict (our install / stock on some paths).
		if (!m_GetCpuClock || !MmIsAddressValid(m_GetCpuClock))
			return false; // cannot probe; Start will fail later

		void* cur = *m_GetCpuClock;

		// Foreign GetCpuClock function pointer that is neither stock nor us
		if (m_BuildNumber <= 18363)
		{
			if (cur != (void*)m_StockGetCpuClock &&
				cur != (void*)SelfGetCpuClock &&
				MmIsAddressValid(cur))
			{
				// Pointer looks like code and not ours → possible foreign IH
				Log("ConflictProbe: foreign GetCpuClock %p stock=%p", cur, (void*)m_StockGetCpuClock);
				return true;
			}
			return false;
		}

		// >18363: if selector is 2 and Hvl points to non-stock non-our fake → conflict
		if ((unsigned long long)cur == 2 && m_HvlGetQpcBias &&
			MmIsAddressValid((void*)m_HvlGetQpcBias))
		{
			unsigned long long hvl = *((unsigned long long*)m_HvlGetQpcBias);
			if (hvl != (unsigned long long)m_OriginalHvlGetQpcBias &&
				hvl != (unsigned long long)FakeHvlGetQpcBias &&
				hvl != 0)
			{
				Log("ConflictProbe: foreign HvlGetQpcBias %p", (void*)hvl);
				return true;
			}
		}

		// If not started and GetCpuClock already SelfGetCpuClock from another module image
		if (!IsStarted && cur == (void*)SelfGetCpuClock)
		{
			// Only conflict if we did not install (should not happen with our Self)
			// Another driver cannot point to our SelfGetCpuClock unless same image — ignore.
		}

		return false;
	}

	bool IsRunning()
	{
		return IsStarted;
	}

	// --- Resolve patterns (no mutation of clocks / no SYSTEMCALL) ---

	static bool ResolvePatterns()
	{
		m_BuildNumber = k_utils::GetSystemBuildNumber();
		Log("build number %ld", m_BuildNumber);
		if (!m_BuildNumber)
			return false;

		unsigned long long ntoskrnl = k_utils::GetModuleAddress("ntoskrnl.exe", nullptr);
		Log("ntoskrnl %llX", ntoskrnl);
		if (!ntoskrnl)
			return false;

		// CKCL session must exist so logger context is valid (no SYSTEMCALL flag)
		if (!EnsureCkclSessionRunning())
		{
			Log("Init FAIL build=%ld symbol=CKCL session", m_BuildNumber);
			return false;
		}

		unsigned long long EtwpDebuggerData =
			k_utils::FindPatternImage(ntoskrnl, "\x00\x00\x2c\x08\x04\x38\x0c", "??xxxxx", ".text");
		if (!EtwpDebuggerData)
			EtwpDebuggerData =
				k_utils::FindPatternImage(ntoskrnl, "\x00\x00\x2c\x08\x04\x38\x0c", "??xxxxx", ".data");
		if (!EtwpDebuggerData)
			EtwpDebuggerData =
				k_utils::FindPatternImage(ntoskrnl, "\x00\x00\x2c\x08\x04\x38\x0c", "??xxxxx", ".rdata");
		if (!EtwpDebuggerData)
		{
			Log("Init FAIL build=%ld symbol=EtwpDebuggerData", m_BuildNumber);
			return false;
		}
		m_EtwpDebuggerData = (void*)EtwpDebuggerData;

		m_EtwpDebuggerDataSilo = *(void***)((unsigned long long)m_EtwpDebuggerData + 0x10);
		if (!m_EtwpDebuggerDataSilo)
		{
			Log("Init FAIL build=%ld symbol=EtwpDebuggerDataSilo", m_BuildNumber);
			return false;
		}

		m_CkclWmiLoggerContext = m_EtwpDebuggerDataSilo[0x2];
		if (!m_CkclWmiLoggerContext)
		{
			Log("Init FAIL build=%ld symbol=CkclWmiLoggerContext", m_BuildNumber);
			return false;
		}

		if (m_BuildNumber <= 7601 || m_BuildNumber >= 22000)
			m_GetCpuClock = (void**)((unsigned long long)m_CkclWmiLoggerContext + 0x18);
		else
			m_GetCpuClock = (void**)((unsigned long long)m_CkclWmiLoggerContext + 0x28);

		if (!MmIsAddressValid(m_GetCpuClock))
		{
			Log("Init FAIL build=%ld symbol=GetCpuClock slot invalid", m_BuildNumber);
			return false;
		}

		m_StockGetCpuClock = (unsigned long long)(*m_GetCpuClock);
		m_OriginalGetCpuClock = m_StockGetCpuClock;
		Log("GetCpuClock slot %p stock %p build=%ld", m_GetCpuClock, (void*)m_StockGetCpuClock, m_BuildNumber);

		m_SystemCallTable = PAGE_ALIGN(k_utils::GetSyscallEntry(ntoskrnl));
		if (!m_SystemCallTable)
		{
			Log("Init FAIL build=%ld symbol=SystemCallTable", m_BuildNumber);
			return false;
		}

		if (m_BuildNumber > 18363)
		{
			unsigned long long addressHvlpReferenceTscPage = k_utils::FindPatternImage(ntoskrnl,
				"\x48\x8b\x05\x00\x00\x00\x00\x48\x8b\x40\x00\x48\x8b\x0d\x00\x00\x00\x00\x48\xf7\xe2",
				"xxx????xxx?xxx????xxx");
			if (!addressHvlpReferenceTscPage)
			{
				Log("Init FAIL build=%ld symbol=HvlpReferenceTscPage", m_BuildNumber);
				return false;
			}
			m_HvlpReferenceTscPage = reinterpret_cast<unsigned long long>(
				reinterpret_cast<char*>(addressHvlpReferenceTscPage) + 7 +
				*reinterpret_cast<int*>(reinterpret_cast<char*>(addressHvlpReferenceTscPage) + 3));
			if (!m_HvlpReferenceTscPage)
				return false;

			unsigned long long addressHvlGetQpcBias = k_utils::FindPatternImage(ntoskrnl,
				"\x48\x8b\x05\x00\x00\x00\x00\x48\x85\xc0\x74\x00\x48\x83\x3d\x00\x00\x00\x00\x00\x74",
				"xxx????xxxx?xxx?????x");
			if (!addressHvlGetQpcBias)
			{
				addressHvlGetQpcBias = k_utils::FindPatternImage(ntoskrnl,
					"\x48\x8b\x05\x00\x00\x00\x00\xe8\x00\x00\x00\x00\x48\x03\xd8\x48\x89\x1f",
					"xxx????x????xxxxxx");
			}
			if (!addressHvlGetQpcBias)
			{
				Log("Init FAIL build=%ld symbol=HvlGetQpcBias", m_BuildNumber);
				return false;
			}
			m_HvlGetQpcBias = reinterpret_cast<unsigned long long>(
				reinterpret_cast<char*>(addressHvlGetQpcBias) + 7 +
				*reinterpret_cast<int*>(reinterpret_cast<char*>(addressHvlGetQpcBias) + 3));
			if (!m_HvlGetQpcBias)
				return false;
			m_OriginalHvlGetQpcBias = (FHvlGetQpcBias)(*((unsigned long long*)m_HvlGetQpcBias));

			unsigned long long addressHvlpGetReferenceTimeUsingTscPage = k_utils::FindPatternImage(ntoskrnl,
				"\x48\x8b\x05\x00\x00\x00\x00\x48\x85\xc0\x74\x00\x33\xc9\xe8\x00\x00\x00\x00\x48\x8b\xd8",
				"xxx????xxxx?xxx????xxx");
			if (!addressHvlpGetReferenceTimeUsingTscPage)
			{
				addressHvlpGetReferenceTimeUsingTscPage = k_utils::FindPatternImage(ntoskrnl,
					"\x48\x8b\x05\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x48\x03\xd8",
					"xxx????x????xxx");
			}
			if (!addressHvlpGetReferenceTimeUsingTscPage)
			{
				Log("Init FAIL build=%ld symbol=HvlpGetReferenceTimeUsingTscPage", m_BuildNumber);
				return false;
			}
			m_HvlpGetReferenceTimeUsingTscPage = (unsigned long long)(
				(char*)(addressHvlpGetReferenceTimeUsingTscPage) + 7 +
				*(int*)((char*)(addressHvlpGetReferenceTimeUsingTscPage) + 3));
			if (!m_HvlpGetReferenceTimeUsingTscPage)
				return false;
			m_OriginalHvlpGetReferenceTimeUsingTscPage =
				*((unsigned long long*)m_HvlpGetReferenceTimeUsingTscPage);

			unsigned long long addressHalpPerformanceCounter = k_utils::FindPatternImage(ntoskrnl,
				"\x48\x8b\x05\x00\x00\x00\x00\x48\x8b\xf9\x48\x85\xc0\x74\x00\x83\xb8",
				"xxx????xxxxxxx?xx");
			if (!addressHalpPerformanceCounter)
			{
				Log("Init FAIL build=%ld symbol=HalpPerformanceCounter", m_BuildNumber);
				return false;
			}
			m_HalpPerformanceCounter = reinterpret_cast<unsigned long long>(
				reinterpret_cast<char*>(addressHalpPerformanceCounter) + 7 +
				*reinterpret_cast<int*>(reinterpret_cast<char*>(addressHalpPerformanceCounter) + 3));
			if (!m_HalpPerformanceCounter)
				return false;

			unsigned long long addressHalpOriginalPerformanceCounter = k_utils::FindPatternImage(ntoskrnl,
				"\x48\x8b\x05\x00\x00\x00\x00\x48\x3b\x00\x0f\x85\x00\x00\x00\x00\xA0",
				"xxx????xx?xx????x");
			if (!addressHalpOriginalPerformanceCounter)
			{
				addressHalpOriginalPerformanceCounter = k_utils::FindPatternImage(ntoskrnl,
					"\x48\x8b\x0d\x00\x00\x00\x00\x4c\x00\x00\x00\x00\x48\x3b\xf1",
					"xxx????x????xxx");
				if (!addressHalpOriginalPerformanceCounter)
				{
					Log("Init FAIL build=%ld symbol=HalpOriginalPerformanceCounter", m_BuildNumber);
					return false;
				}
			}
			m_HalpOriginalPerformanceCounter = reinterpret_cast<unsigned long long>(
				reinterpret_cast<char*>(addressHalpOriginalPerformanceCounter) + 7 +
				*reinterpret_cast<int*>(reinterpret_cast<char*>(addressHalpOriginalPerformanceCounter) + 3));
			if (!m_HalpOriginalPerformanceCounter)
				return false;

			m_HalpPerformanceCounterType =
				(ULONG*)((ULONG_PTR)(*(PVOID*)m_HalpPerformanceCounter) + HALP_PERFORMANCE_COUNTER_TYPE_OFFSET);
			if (!m_HalpPerformanceCounterType)
				return false;

			m_OriginalHalpPerformanceCounterType = *m_HalpPerformanceCounterType;
			m_PhysicalMachineHalpPath =
				(*m_HalpPerformanceCounterType == HALP_PERFORMANCE_COUNTER_TYPE_PHYSICAL_MACHINE);

			if (m_PhysicalMachineHalpPath)
			{
				m_VmHalpPerformanceCounterType =
					*(reinterpret_cast<char*>(addressHalpPerformanceCounter) + 21);

				if (!m_HalpOriginalPerformanceCounterCopy)
				{
					m_HalpOriginalPerformanceCounterCopy =
						(ULONGLONG)ExAllocatePoolWithTag(NonPagedPool, 0xFF, kPoolTag);
					if (!m_HalpOriginalPerformanceCounterCopy)
						return false;
					RtlZeroMemory((PVOID)m_HalpOriginalPerformanceCounterCopy, 0xFF);
					*(PULONGLONG)(m_HalpOriginalPerformanceCounterCopy + HALP_PERFORMANCE_COUNTER_BASE_RATE_OFFSET) =
						HALP_PERFORMANCE_COUNTER_BASE_RATE;
					*(PULONG)(m_HalpOriginalPerformanceCounterCopy + HALP_PERFORMANCE_COUNTER_TYPE_OFFSET) =
						HALP_PERFORMANCE_COUNTER_TYPE_PHYSICAL_MACHINE;
				}

				if (!m_QpcMdl)
				{
					PLONGLONG pQpcPointer = (PLONGLONG)0xFFFFF780000003B8;
					m_QpcMdl = IoAllocateMdl(pQpcPointer, 8, false, false, NULL);
					if (!m_QpcMdl)
						return false;
					MmBuildMdlForNonPagedPool(m_QpcMdl);
					m_QpcPointer = (PLONGLONG)MmMapLockedPagesSpecifyCache(
						m_QpcMdl, KernelMode, MmWriteCombined, NULL, false, NormalPagePriority);
					if (!m_QpcPointer)
						return false;
				}
			}
		}

		return true;
	}

	// --- Install / restore clocks ---

	static bool InstallClocks()
	{
		if (!MmIsAddressValid(m_GetCpuClock))
			return false;

		m_OriginalGetCpuClock = (unsigned long long)(*m_GetCpuClock);

		if (m_BuildNumber <= 18363)
		{
			*m_GetCpuClock = SelfGetCpuClock;
			Log("Install GetCpuClock -> SelfGetCpuClock");
		}
		else
		{
			*m_GetCpuClock = (void*)2;
			Log("Install GetCpuClock -> 2");

			m_OriginalHvlGetQpcBias = (FHvlGetQpcBias)(*((unsigned long long*)m_HvlGetQpcBias));

			if (m_HvlpGetReferenceTimeUsingTscPage)
			{
				m_OriginalHvlpGetReferenceTimeUsingTscPage =
					*((unsigned long long*)m_HvlpGetReferenceTimeUsingTscPage);
				if (m_OriginalHvlpGetReferenceTimeUsingTscPage == 0)
				{
					*((unsigned long long*)m_HvlpGetReferenceTimeUsingTscPage) =
						(ULONGLONG)FakeGetReferenceTimeUsingTscPage;
					m_HvlpFakeInstalled = true;
					Log("Install FakeGetReferenceTimeUsingTscPage");
				}
			}

			if (m_PhysicalMachineHalpPath && m_HalpPerformanceCounterType)
			{
				m_OriginalHalpPerformanceCounterType = *m_HalpPerformanceCounterType;
				*(unsigned long long*)m_HalpOriginalPerformanceCounter = m_HalpOriginalPerformanceCounterCopy;
				*m_HalpPerformanceCounterType = m_VmHalpPerformanceCounterType;
				Log("Install Halp physical-machine path type=%u", m_VmHalpPerformanceCounterType);
			}

			*((unsigned long long*)m_HvlGetQpcBias) = (unsigned long long)FakeHvlGetQpcBias;
			Log("Install FakeHvlGetQpcBias");
		}

		m_ClocksInstalled = true;
		return true;
	}

	static void RestoreClocks()
	{
		if (!m_GetCpuClock || !MmIsAddressValid(m_GetCpuClock))
		{
			m_ClocksInstalled = false;
			return;
		}

		*m_GetCpuClock = (void*)m_OriginalGetCpuClock;
		Log("Restore GetCpuClock %p", (void*)m_OriginalGetCpuClock);

		if (m_BuildNumber > 18363)
		{
			if (m_HvlpGetReferenceTimeUsingTscPage && m_HvlpFakeInstalled)
			{
				*((unsigned long long*)m_HvlpGetReferenceTimeUsingTscPage) =
					m_OriginalHvlpGetReferenceTimeUsingTscPage;
				m_HvlpFakeInstalled = false;
			}

			if (m_PhysicalMachineHalpPath && m_HalpPerformanceCounterType)
			{
				LARGE_INTEGER liBegin = KeQueryPerformanceCounter(NULL);
				*m_HalpPerformanceCounterType = m_OriginalHalpPerformanceCounterType;
				LARGE_INTEGER liEndFix = KeQueryPerformanceCounter(NULL);
				if (m_QpcPointer &&
					liEndFix.QuadPart - liBegin.QuadPart > HALP_PERFORMANCE_COUNTER_BASE_RATE)
				{
					LONGLONG llQpcValue = *m_QpcPointer;
					llQpcValue -= liEndFix.QuadPart - liBegin.QuadPart;
					*m_QpcPointer = llQpcValue;
				}
			}

			if (m_HvlGetQpcBias)
			{
				*((unsigned long long*)m_HvlGetQpcBias) = (unsigned long long)m_OriginalHvlGetQpcBias;
				Log("Restore HvlGetQpcBias");
			}
		}

		m_ClocksInstalled = false;
	}

	static void StopDetectThread_NoLock()
	{
		m_DetectThreadStatus = false;
		if (m_DetectThreadObject)
		{
			KeWaitForSingleObject(m_DetectThreadObject, Executive, KernelMode, false, NULL);

			UNICODE_STRING usObf = RTL_CONSTANT_STRING(L"ObfDereferenceObject");
			auto fnObf = (ObfDereferenceObjectPtr)MmGetSystemRoutineAddress(&usObf);
			if (fnObf)
				fnObf(m_DetectThreadObject);
			else
			{
				UNICODE_STRING usOb = RTL_CONSTANT_STRING(L"ObDereferenceObject");
				auto fnOb = (ObDereferenceObjectPtr)MmGetSystemRoutineAddress(&usOb);
				if (fnOb)
					fnOb(m_DetectThreadObject);
			}
			m_DetectThreadObject = NULL;
			Log("DetectThread stopped");
		}
	}

	static void DetectThreadRoutine(void*)
	{
		while (m_DetectThreadStatus)
		{
			k_utils::Sleep(1000);
			if (!IsStarted)
				continue;
			if (!LayerBIntact())
			{
				Log("DetectThread: layer B stripped, Repair");
				Repair();
			}
		}
		PsTerminateSystemThread(STATUS_SUCCESS);
	}

	static bool EnsureDetectThread_NoLock()
	{
		if (m_DetectThreadObject)
			return true;

		m_DetectThreadStatus = true;
		OBJECT_ATTRIBUTES att{ 0 };
		HANDLE hThread = NULL;
		InitializeObjectAttributes(&att, 0, OBJ_KERNEL_HANDLE, 0, 0);
		NTSTATUS st = PsCreateSystemThread(
			&hThread, THREAD_ALL_ACCESS, &att, 0, &m_ClientId, DetectThreadRoutine, 0);
		if (!NT_SUCCESS(st))
		{
			Log("Create DetectThread failed %08X", st);
			m_DetectThreadStatus = false;
			return false;
		}
		ObReferenceObjectByHandle(hThread, THREAD_ALL_ACCESS, NULL, KernelMode,
			(PVOID*)&m_DetectThreadObject, NULL);
		ZwClose(hThread);
		Log("DetectThread tid=%d obj=%p", (int)(ULONG_PTR)m_ClientId.UniqueThread, m_DetectThreadObject);
		return true;
	}

	static void RollbackStartPartial_NoLock()
	{
		Log("RollbackStartPartial begin");
		// 1 detect thread if created this attempt
		StopDetectThread_NoLock();
		// 2-5 clocks
		if (m_ClocksInstalled)
			RestoreClocks();
		// 6 CKCL
		if (m_CkclSyscallEnabled)
			DisableCkclSyscall();
		// 7 state
		IsStarted = false;
		m_Ready = false; // force full Initialize next time
		Log("RollbackStartPartial done");
	}

	// --- Public API ---

	bool Initialize(InfinityCallbackPtr pCallback)
	{
		EnsureLifeMutex();
		KeAcquireGuardedMutex(&m_LifeMutex);

		if (IsStarted)
		{
			// Already running — keep callback update only
			if (MmIsAddressValid(pCallback))
				m_InfinityCallback = pCallback;
			KeReleaseGuardedMutex(&m_LifeMutex);
			return true;
		}

		if (!MmIsAddressValid(pCallback))
		{
			KeReleaseGuardedMutex(&m_LifeMutex);
			return false;
		}
		m_InfinityCallback = pCallback;

		// Ready only: NO SYSTEMCALL enable, NO clock install
		if (!ResolvePatterns())
		{
			m_Ready = false;
			KeReleaseGuardedMutex(&m_LifeMutex);
			return false;
		}

		m_Ready = true;
		IsStarted = false;
		Log("Initialize OK (Ready, no layer B yet)");
		KeReleaseGuardedMutex(&m_LifeMutex);
		return true;
	}

	bool Repair()
	{
		EnsureLifeMutex();
		KeAcquireGuardedMutex(&m_LifeMutex);

		if (!IsStarted)
		{
			KeReleaseGuardedMutex(&m_LifeMutex);
			return false;
		}
		if (LayerBIntact())
		{
			KeReleaseGuardedMutex(&m_LifeMutex);
			return true;
		}

		Log("Repair: reinstall clocks");
		// Do not stop DetectThread; do not clear IsStarted
		bool ok = InstallClocks();
		if (!ok)
			Log("Repair failed");
		KeReleaseGuardedMutex(&m_LifeMutex);
		return ok;
	}

	bool Start()
	{
		EnsureLifeMutex();
		KeAcquireGuardedMutex(&m_LifeMutex);

		if (!m_Ready || !m_InfinityCallback)
		{
			Log("Start: not Ready");
			KeReleaseGuardedMutex(&m_LifeMutex);
			return false;
		}

		if (IsStarted)
		{
			if (LayerBIntact())
			{
				KeReleaseGuardedMutex(&m_LifeMutex);
				return true; // true idempotent
			}
			// Layer B stripped — Repair under same lock (inline)
			Log("Start: IsStarted but layer B bad → Repair");
			bool ok = InstallClocks();
			KeReleaseGuardedMutex(&m_LifeMutex);
			return ok;
		}

		// Cold start
		if (ConflictProbe())
		{
			// 与 CR 等第二套 InfinityHook 并存：硬失败（设计 K19 / 互斥）
			Log("Start: FAIL reason=ConflictProbe (foreign IH / clock owner)");
			KeReleaseGuardedMutex(&m_LifeMutex);
			return false;
		}

		if (!EnableCkclSyscall())
		{
			Log("Start: FAIL reason=EnableCkclSyscall");
			RollbackStartPartial_NoLock();
			KeReleaseGuardedMutex(&m_LifeMutex);
			return false;
		}

		// Re-resolve GetCpuClock slot after enabling SYSTEMCALL (context may update)
		if (!MmIsAddressValid(m_GetCpuClock))
		{
			Log("Start: FAIL reason=GetCpuClock invalid after CKCL");
			RollbackStartPartial_NoLock();
			KeReleaseGuardedMutex(&m_LifeMutex);
			return false;
		}
		m_OriginalGetCpuClock = (unsigned long long)(*m_GetCpuClock);

		if (!InstallClocks())
		{
			Log("Start: FAIL reason=InstallClocks (pattern/slot write)");
			RollbackStartPartial_NoLock();
			KeReleaseGuardedMutex(&m_LifeMutex);
			return false;
		}

		if (!EnsureDetectThread_NoLock())
		{
			Log("Start: FAIL reason=EnsureDetectThread");
			RollbackStartPartial_NoLock();
			KeReleaseGuardedMutex(&m_LifeMutex);
			return false;
		}

		IsStarted = true; // K13: set only after full success
		Log("Start OK IsStarted=true");
		KeReleaseGuardedMutex(&m_LifeMutex);
		return true;
	}

	bool Stop()
	{
		EnsureLifeMutex();

		// Avoid deadlock: DetectThread may be blocked in Repair() holding m_LifeMutex.
		// Signal exit and clear IsStarted first, then wait OUTSIDE the mutex.
		PETHREAD thr = nullptr;
		KeAcquireGuardedMutex(&m_LifeMutex);
		if (!IsStarted)
		{
			KeReleaseGuardedMutex(&m_LifeMutex);
			return true;
		}
		Log("Stop begin");
		IsStarted = false;
		m_DetectThreadStatus = false;
		thr = m_DetectThreadObject;
		m_DetectThreadObject = nullptr;
		KeReleaseGuardedMutex(&m_LifeMutex);

		if (thr)
		{
			KeWaitForSingleObject(thr, Executive, KernelMode, false, NULL);
			UNICODE_STRING usObf = RTL_CONSTANT_STRING(L"ObfDereferenceObject");
			auto fnObf = (ObfDereferenceObjectPtr)MmGetSystemRoutineAddress(&usObf);
			if (fnObf)
				fnObf(thr);
			else
			{
				UNICODE_STRING usOb = RTL_CONSTANT_STRING(L"ObDereferenceObject");
				auto fnOb = (ObDereferenceObjectPtr)MmGetSystemRoutineAddress(&usOb);
				if (fnOb)
					fnOb(thr);
			}
			Log("DetectThread stopped");
		}

		KeAcquireGuardedMutex(&m_LifeMutex);
		if (m_ClocksInstalled)
			RestoreClocks();
		if (m_CkclSyscallEnabled)
			DisableCkclSyscall();
		m_Ready = false;
		Log("Stop done");
		KeReleaseGuardedMutex(&m_LifeMutex);
		return true;
	}

	bool Cleanup()
	{
		// Full stop if running (handles thread wait safely)
		if (IsStarted)
			return Stop();

		EnsureLifeMutex();
		KeAcquireGuardedMutex(&m_LifeMutex);
		// Partial / Ready-only residue (no DetectThread expected)
		if (m_ClocksInstalled)
			RestoreClocks();
		if (m_CkclSyscallEnabled)
			DisableCkclSyscall();
		// orphan detect thread?
		if (m_DetectThreadObject)
		{
			m_DetectThreadStatus = false;
			PETHREAD thr = m_DetectThreadObject;
			m_DetectThreadObject = nullptr;
			KeReleaseGuardedMutex(&m_LifeMutex);
			KeWaitForSingleObject(thr, Executive, KernelMode, false, NULL);
			UNICODE_STRING usObf = RTL_CONSTANT_STRING(L"ObfDereferenceObject");
			auto fnObf = (ObfDereferenceObjectPtr)MmGetSystemRoutineAddress(&usObf);
			if (fnObf)
				fnObf(thr);
			KeAcquireGuardedMutex(&m_LifeMutex);
		}
		m_Ready = false;
		IsStarted = false;
		Log("Cleanup partial/Ready");
		KeReleaseGuardedMutex(&m_LifeMutex);
		return true;
	}
}
