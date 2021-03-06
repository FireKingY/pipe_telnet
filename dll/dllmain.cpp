
#include "stdafx.h"


BOOL APIENTRY InstallHook();
BOOL APIENTRY UnInstallHook();

void* hConsoleOutput = (void*)0x2b79C;
void* hConsoleInput = (void*)0x2b798;
void* _g_hSessionConsoleBuffer = (void*)0x25d18;
std::vector<PVOID> news;
std::vector<PVOID*> olds;
void init();


BOOL(WINAPI *oldHeapSetInformation)(_In_opt_ HANDLE HeapHandle, _In_ HEAP_INFORMATION_CLASS HeapInformationClass, _In_reads_bytes_opt_(HeapInformationLength) PVOID HeapInformation, _In_ SIZE_T HeapInformationLength) = HeapSetInformation;
BOOL WINAPI newHeapSetInformation(_In_opt_ HANDLE HeapHandle, _In_ HEAP_INFORMATION_CLASS HeapInformationClass, _In_reads_bytes_opt_(HeapInformationLength) PVOID HeapInformation, _In_ SIZE_T HeapInformationLength)
{
	char16_t* basename;

	//这里最初是没有关闭重定位的，下面汇编是拿到随机加载的基址，并计算出正确的iHandle，oHandle的地址(基址+RVA)。
	_asm
	{
		push eax;
		push ecx;
		mov eax, fs:[30h];
		mov eax, [eax + 0ch];
		mov eax, [eax + 0ch];
		mov ecx, [eax + 0x18]; //ecx = BaseAddr
		add hConsoleInput, ecx;
		add hConsoleOutput, ecx;
		add _g_hSessionConsoleBuffer, ecx;
		mov ecx, [eax + 0x30];
		mov basename, ecx;
		pop ecx;
		pop eax;
	}

	//创建标准console HANDLE并赋给telnet的全局变量
	SECURITY_ATTRIBUTES sa = { 0 };
	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = true;
	*(HANDLE*)hConsoleOutput = CreateFile(L"CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, NULL, NULL);
	*(HANDLE*)hConsoleInput = CreateFile(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, NULL, NULL);
	*(HANDLE*)_g_hSessionConsoleBuffer = *(HANDLE*)hConsoleInput;

	//调用原来的函数，完成初始化
	return oldHeapSetInformation(HeapHandle, HeapInformationClass, HeapInformation, HeapInformationLength);
}

BOOL (WINAPI *oldGetConsoleScreenBufferInfo)(_In_ HANDLE hConsoleOutput, _Out_ PCONSOLE_SCREEN_BUFFER_INFO lpConsoleScreenBufferInfo) = GetConsoleScreenBufferInfo;
BOOL WINAPI newConsoleScreenBufferInfo(_In_ HANDLE hConsoleOutput, _Out_ PCONSOLE_SCREEN_BUFFER_INFO lpConsoleScreenBufferInfo)
{
	//修改传入参数
	return oldGetConsoleScreenBufferInfo(*(HANDLE*)_g_hSessionConsoleBuffer, lpConsoleScreenBufferInfo);
}

BOOL(WINAPI *oldWriteConsoleW)(_In_ HANDLE hConsoleOutput, _In_reads_(nNumberOfCharsToWrite) CONST VOID * lpBuffer, _In_ DWORD nNumberOfCharsToWrite, _Out_opt_ LPDWORD lpNumberOfCharsWritten, _Reserved_ LPVOID lpReserved) = WriteConsoleW;
BOOL WINAPI newWriteConsoleW(_In_ HANDLE hConsoleOutput, _In_reads_(nNumberOfCharsToWrite) CONST VOID * lpBuffer, _In_ DWORD nNumberOfCharsToWrite, _Out_opt_ LPDWORD lpNumberOfCharsWritten, _Reserved_ LPVOID lpReserved)
{
	//write output to pipe
	DWORD writed;
	WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), lpBuffer, nNumberOfCharsToWrite * 2, &writed, NULL);
	return true;
}

BOOL (WINAPI *oldReadConsoleW)(_In_ HANDLE hConsoleInput, _Out_writes_bytes_to_(nNumberOfCharsToRead * sizeof(WCHAR), *lpNumberOfCharsRead * sizeof(WCHAR)) LPVOID lpBuffer, _In_ DWORD nNumberOfCharsToRead, _Out_ _Deref_out_range_(<= , nNumberOfCharsToRead) LPDWORD lpNumberOfCharsRead, _In_opt_ PCONSOLE_READCONSOLE_CONTROL pInputControl)= ReadConsoleW;
BOOL WINAPI newReadConsoleW(_In_ HANDLE hConsoleInput, _Out_writes_bytes_to_(nNumberOfCharsToRead * sizeof(WCHAR), *lpNumberOfCharsRead * sizeof(WCHAR)) LPVOID lpBuffer, _In_ DWORD nNumberOfCharsToRead, _Out_ _Deref_out_range_(<= , nNumberOfCharsToRead) LPDWORD lpNumberOfCharsRead, _In_opt_ PCONSOLE_READCONSOLE_CONTROL pInputControl)
{
	//read input from pipe
	PeekNamedPipe(GetStdHandle(STD_INPUT_HANDLE), lpBuffer, nNumberOfCharsToRead, 0, lpNumberOfCharsRead, NULL);
	if (lpNumberOfCharsRead)
		ReadFile(GetStdHandle(STD_INPUT_HANDLE), lpBuffer, *lpNumberOfCharsRead, lpNumberOfCharsRead, NULL);
	return true;
}

BOOL APIENTRY DllMain(HMODULE hModule,DWORD  ul_reason_for_call,LPVOID lpReserved)
{
	init();
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
		InstallHook();
	else if (ul_reason_for_call == DLL_PROCESS_DETACH)
		UnInstallHook();
	return true;
}

void init()
{
	
	olds.push_back((PVOID*)&oldHeapSetInformation);
	news.push_back((PVOID)newHeapSetInformation);
	olds.push_back((PVOID*)&oldGetConsoleScreenBufferInfo);
	news.push_back((PVOID)newConsoleScreenBufferInfo);
	olds.push_back((PVOID*)&oldWriteConsoleW);
	news.push_back((PVOID)newWriteConsoleW);
	olds.push_back((PVOID*)&oldReadConsoleW);
	news.push_back((PVOID)newReadConsoleW);
}

BOOL APIENTRY InstallHook()
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	for(unsigned int i=0;i<olds.size();++i)
		DetourAttach(olds[i], news[i]);
	LONG ret = DetourTransactionCommit();
	return ret == NO_ERROR;
}

BOOL APIENTRY UnInstallHook()
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	for (unsigned int i = 0; i<olds.size(); ++i)
		DetourDetach(olds[i], news[i]);
	LONG ret = DetourTransactionCommit();
	return ret == NO_ERROR;
}
