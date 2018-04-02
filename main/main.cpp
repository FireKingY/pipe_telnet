#include<stdio.h>
#include<Windows.h>
#include<WinBase.h>
#include <detours.h>  

#define INPUT_BUFFER_SIZE 1024
#define OUTPUT_BUFFER_SIZE 1024

bool printMsg(HANDLE outputRead);

int main()
{

	HANDLE inputRead = NULL;
	HANDLE inputWrite = NULL;
	HANDLE outputRead = NULL;
	HANDLE outputWrite = NULL;

	SECURITY_ATTRIBUTES sa = { 0 };
	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = true;

	//create pipe
	if (!CreatePipe(&inputRead, &inputWrite, &sa, 0))
		return GetLastError();
	if (!CreatePipe(&outputRead, &outputWrite, &sa, 0))
		return GetLastError();

	
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	memset(&si, 0, sizeof(STARTUPINFO));
	memset(&pi, 0, sizeof(PROCESS_INFORMATION));

	si.cb = sizeof(STARTUPINFO);
	si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	si.hStdError = GetStdHandle(STD_OUTPUT_HANDLE);
	si.hStdInput = inputRead;
	si.hStdOutput = outputWrite;
	si.hStdError = outputWrite;
	si.wShowWindow = SW_HIDE;
	si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
	char name[] = "telnet";
	char DLLPath[] = "C:\\Users\\Fire\\source\\repos\\Dll1\\Debug\\Dll1.dll";
	//create telnet processs
	if (!DetourCreateProcessWithDll(NULL, name, NULL, NULL, TRUE, NORMAL_PRIORITY_CLASS|CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi,DLLPath, NULL))
	{
		DWORD errCode;
		errCode = GetLastError();
		printf("create process failed\n code:%d\n", errCode);
		return errCode;
	}

	//print start info
	Sleep(100);
	printMsg(outputRead);

	DWORD exitCode;
	if (GetExitCodeProcess(pi.hProcess, &exitCode) && exitCode != 259)
	{
		printf("telent has exited\nexitCode:%d\n", exitCode);
		return 0;
	}

	//read loop
	char16_t inputBuffer[INPUT_BUFFER_SIZE];
	DWORD writed;
	DWORD readed;
	while (true)
	{
		ZeroMemory((void*)inputBuffer, INPUT_BUFFER_SIZE*sizeof(char16_t));
		//read input to inputBuffer
		ReadConsoleW(GetStdHandle(STD_INPUT_HANDLE), inputBuffer, INPUT_BUFFER_SIZE-1, &readed, NULL);
		//write input to pipe
		WriteFile(inputWrite, inputBuffer, readed, &writed, NULL);

		Sleep(100);
		if (GetExitCodeProcess(pi.hProcess, &exitCode) && exitCode != 259)
		{
			printf("telent has exited\nexitCode:%d\n", exitCode);
			break;
		}
		printMsg(outputRead);
	}
	
	return 0;
}

bool printMsg(HANDLE outputRead)
{
	
	char WideCharBuffer[OUTPUT_BUFFER_SIZE] = {0};
	char MultiByteBufffer[OUTPUT_BUFFER_SIZE] = { 0 };
	DWORD readed=0;
	DWORD readAble = 0;

	PeekNamedPipe(outputRead, WideCharBuffer, 1023, 0, &readAble, NULL);
	//if there is some output
	while (readAble)
	{
		ZeroMemory(WideCharBuffer, OUTPUT_BUFFER_SIZE);
		ZeroMemory(MultiByteBufffer, OUTPUT_BUFFER_SIZE);
		ReadFile(outputRead, WideCharBuffer, readAble, &readed, NULL);
		WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)WideCharBuffer, -1, MultiByteBufffer, readAble, NULL, NULL);
		printf("%s", MultiByteBufffer);	

		//check if there remains output
		PeekNamedPipe(outputRead, WideCharBuffer, 1023, 0, &readAble, NULL);
	}
	return true;
}
