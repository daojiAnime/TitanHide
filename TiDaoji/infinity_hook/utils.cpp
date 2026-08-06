#include "utils.hpp"
#include <ntstrsafe.h>

namespace k_utils
{
	unsigned long GetSystemBuildNumber()
	{
		unsigned long nNumber = 0;
		RTL_OSVERSIONINFOEXW info;
		RtlZeroMemory(&info, sizeof(info));
		info.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOEXW);
		if (NT_SUCCESS(RtlGetVersion((PRTL_OSVERSIONINFOW)&info)))
			nNumber = info.dwBuildNumber;
		return nNumber;
	}

	unsigned long long GetModuleAddress(const char* szName, unsigned long* nSize)
	{
		unsigned long long nResult = 0;
		unsigned long nLength = 0;
		ZwQuerySystemInformation(SystemModuleInformation, &nLength, 0, &nLength);
		if (!nLength)
			return nResult;

		const unsigned long nTag = 'Tdji';
		PSYSTEM_MODULE_INFORMATION pSystemModules =
			(PSYSTEM_MODULE_INFORMATION)ExAllocatePoolWithTag(NonPagedPool, nLength, nTag);
		if (!pSystemModules)
			return nResult;

		NTSTATUS nStatus = ZwQuerySystemInformation(SystemModuleInformation, pSystemModules, nLength, 0);
		if (NT_SUCCESS(nStatus))
		{
			for (unsigned long long i = 0; i < pSystemModules->ulModuleCount; i++)
			{
				PSYSTEM_MODULE_INFORMATION_ENTRY pMod = &pSystemModules->Modules[i];
				if (strstr(pMod->ImageName, szName))
				{
					nResult = (unsigned long long)pMod->Base;
					if (nSize)
						*nSize = (unsigned long)pMod->Size;
					break;
				}
			}
		}

		ExFreePoolWithTag(pSystemModules, nTag);
		return nResult;
	}

	bool PatternCheck(const char* pData, const char* szPattern, const char* szMask)
	{
		size_t nLen = strlen(szMask);
		for (size_t i = 0; i < nLen; i++)
		{
			if (pData[i] == szPattern[i] || szMask[i] == '?')
				continue;
			return false;
		}
		return true;
	}

	unsigned long long FindPattern(unsigned long long pAddress, unsigned long nSize, const char* szPattern, const char* szMask)
	{
		nSize -= (unsigned long)strlen(szMask);
		for (unsigned long i = 0; i < nSize; i++)
		{
			if (PatternCheck((const char*)pAddress + i, szPattern, szMask))
				return pAddress + i;
		}
		return 0;
	}

	unsigned long long FindPatternImage(unsigned long long pAddress, const char* szPattern, const char* szMask, const char* szSectionName)
	{
		PIMAGE_DOS_HEADER pImageDosHeader = (PIMAGE_DOS_HEADER)pAddress;
		if (pImageDosHeader->e_magic != IMAGE_DOS_SIGNATURE)
			return 0;

		PIMAGE_NT_HEADERS64 pImageNtHeader = (PIMAGE_NT_HEADERS64)(pAddress + pImageDosHeader->e_lfanew);
		if (pImageNtHeader->Signature != IMAGE_NT_SIGNATURE)
			return 0;

		PIMAGE_SECTION_HEADER pImageSectionHeader = IMAGE_FIRST_SECTION(pImageNtHeader);
		for (unsigned short i = 0; i < pImageNtHeader->FileHeader.NumberOfSections; i++)
		{
			PIMAGE_SECTION_HEADER p = &pImageSectionHeader[i];
			if (strstr((const char*)p->Name, szSectionName))
			{
				unsigned long long nResult =
					FindPattern(pAddress + p->VirtualAddress, p->Misc.VirtualSize, szPattern, szMask);
				if (nResult)
					return nResult;
			}
		}
		return 0;
	}

	unsigned long long GetImageSectionAddress(unsigned long long pAddress, const char* szSectionName, unsigned long* nSize)
	{
		PIMAGE_DOS_HEADER pImageDosHeader = (PIMAGE_DOS_HEADER)pAddress;
		if (pImageDosHeader->e_magic != IMAGE_DOS_SIGNATURE)
			return 0;

		PIMAGE_NT_HEADERS64 pImageNtHeader = (PIMAGE_NT_HEADERS64)(pAddress + pImageDosHeader->e_lfanew);
		if (pImageNtHeader->Signature != IMAGE_NT_SIGNATURE)
			return 0;

		PIMAGE_SECTION_HEADER pImageSectionHeader = IMAGE_FIRST_SECTION(pImageNtHeader);
		for (unsigned short i = 0; i < pImageNtHeader->FileHeader.NumberOfSections; i++)
		{
			PIMAGE_SECTION_HEADER p = &pImageSectionHeader[i];
			if (strstr((const char*)p->Name, szSectionName))
			{
				if (nSize)
					*nSize = p->SizeOfRawData;
				return pAddress + p->VirtualAddress;
			}
		}
		return 0;
	}

	void* GetSyscallEntry(unsigned long long ntoskrnl)
	{
		if (!ntoskrnl)
			return nullptr;

#define IA32_LSTAR_MSR 0xC0000082
		void* pSyscallEntry = (void*)__readmsr(IA32_LSTAR_MSR);

		unsigned long nSectionSize = 0;
		unsigned long long pKVASCODE = GetImageSectionAddress(ntoskrnl, "KVASCODE", &nSectionSize);
		if (!pKVASCODE)
			return pSyscallEntry;

		if (!(pSyscallEntry >= (void*)pKVASCODE &&
			pSyscallEntry < (void*)(pKVASCODE + nSectionSize)))
			return pSyscallEntry;

		hde64s hdeInfo;
		RtlZeroMemory(&hdeInfo, sizeof(hdeInfo));
		for (char* pKiSystemServiceUser = (char*)pSyscallEntry; ; pKiSystemServiceUser += hdeInfo.len)
		{
			if (!hde64_disasm(pKiSystemServiceUser, &hdeInfo))
				break;

#define OPCODE_JMP_NEAR 0xE9
			if (hdeInfo.opcode != OPCODE_JMP_NEAR)
				continue;

			void* pPossibleSyscallEntry =
				(void*)((long long)pKiSystemServiceUser + (int)hdeInfo.len + (int)hdeInfo.imm.imm32);
			if (pPossibleSyscallEntry >= (void*)pKVASCODE &&
				pPossibleSyscallEntry < (void*)((unsigned long long)pKVASCODE + nSectionSize))
				continue;

			pSyscallEntry = pPossibleSyscallEntry;
			break;
		}
		return pSyscallEntry;
	}

	void Sleep(long msec)
	{
		LARGE_INTEGER liDelay;
		liDelay.QuadPart = -10000;
		liDelay.QuadPart *= msec;
		KeDelayExecutionThread(KernelMode, FALSE, &liDelay);
	}
}
