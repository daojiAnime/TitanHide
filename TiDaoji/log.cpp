#include "log.h"

static UNICODE_STRING LogFilename;
static wchar_t LogFilenameBuffer[256];

void InitLog(const PUNICODE_STRING DriverName)
{
	RtlInitEmptyUnicodeString(&LogFilename, LogFilenameBuffer, sizeof(LogFilenameBuffer));
	RtlAppendUnicodeToString(&LogFilename, L"\\DosDevices\\C:\\");
	RtlAppendUnicodeStringToString(&LogFilename, DriverName);
	RtlAppendUnicodeToString(&LogFilename, L".log");
	Log("[TIDAOJI] Log file initialized\r\n");
}

void Log(const char* format, ...)
{
	// Avoid CRT / ntstrsafe worker links that break on incomplete WDK MSBuild.
	// Kernel DbgPrintEx is always available from ntoskrnl.
	va_list vl;
	va_start(vl, format);
	// No portable vDbgPrintEx on all WDK; emit format string only (args ignored for file).
	// Call sites still use printf-style strings for human readers.
	UNREFERENCED_PARAMETER(vl);
	va_end(vl);

	if (!format)
		return;

	DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "%s", format);

	if (KeGetCurrentIrql() != PASSIVE_LEVEL)
		return;

	char msg[1024];
	SIZE_T i = 0;
	for (; format[i] != '\0' && i + 1 < sizeof(msg); i++)
		msg[i] = format[i];
	msg[i] = '\0';

	OBJECT_ATTRIBUTES objAttr;
	InitializeObjectAttributes(&objAttr, &LogFilename,
		OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
		NULL, NULL);

	HANDLE handle;
	IO_STATUS_BLOCK ioStatusBlock;
	NTSTATUS ntstatus = ZwCreateFile(&handle,
		FILE_APPEND_DATA,
		&objAttr, &ioStatusBlock, NULL,
		FILE_ATTRIBUTE_NORMAL,
		FILE_SHARE_WRITE | FILE_SHARE_READ,
		FILE_OPEN_IF,
		FILE_SYNCHRONOUS_IO_NONALERT,
		NULL, 0);
	if (NT_SUCCESS(ntstatus))
	{
		SIZE_T cb = i;
		ZwWriteFile(handle, NULL, NULL, NULL, &ioStatusBlock, msg, (ULONG)cb, NULL, NULL);
		ZwClose(handle);
	}
}
