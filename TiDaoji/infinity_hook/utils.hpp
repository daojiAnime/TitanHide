#pragma once
#include "headers.h"
#include "defines.h"
#include "hde/hde64.h"

#ifdef __cplusplus
extern "C"
{
#endif
	NTSTATUS NTAPI ZwQuerySystemInformation(
		SYSTEM_INFORMATION_CLASS systemInformationClass,
		PVOID systemInformation,
		ULONG systemInformationLength,
		PULONG returnLength);

	NTSTATUS NTAPI NtTraceControl(
		ULONG FunctionCode,
		PVOID InBuffer,
		ULONG InBufferLen,
		PVOID OutBuffer,
		ULONG OutBufferLen,
		PULONG ReturnLength);
#ifdef __cplusplus
}
#endif

namespace k_utils
{
	unsigned long GetSystemBuildNumber();
	unsigned long long GetModuleAddress(const char* szName, unsigned long* nSize);
	bool PatternCheck(const char* pData, const char* szPattern, const char* szMask);
	unsigned long long FindPattern(unsigned long long pAddress, unsigned long nSize, const char* szPattern, const char* szMask);
	unsigned long long FindPatternImage(unsigned long long pAddress, const char* szPattern, const char* szMask, const char* szSectionName = ".text");
	unsigned long long GetImageSectionAddress(unsigned long long pAddress, const char* szSectionName, unsigned long* nSize);
	void* GetSyscallEntry(unsigned long long ntoskrnl);
	void Sleep(long msec);
}
