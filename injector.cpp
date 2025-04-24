#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <iostream>

// Find process ID by executable name
DWORD FindProcessId(const std::wstring& processName) {
    PROCESSENTRY32 pe32 = { sizeof(pe32) };
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return 0;

    if (Process32First(hSnapshot, &pe32)) {
        do {
            if (_wcsicmp(pe32.szExeFile, processName.c_str()) == 0) {
                CloseHandle(hSnapshot);
                return pe32.th32ProcessID;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }
    CloseHandle(hSnapshot);
    return 0;
}

int main() {
    // Target process executable name (e.g., "notepad.exe")
    std::wstring targetProcess = L"notepad.exe";
    // Full path to the DLL to inject
    std::wstring dllPath = L"C:\\Path\\To\\Your\\HookDll.dll";

    // Find the target process ID
    DWORD processId = FindProcessId(targetProcess);
    if (processId == 0) {
        std::wcerr << L"Process not found: " << targetProcess << std::endl;
        return 1;
    }

    // Open the target process
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (!hProcess) {
        std::wcerr << L"Failed to open process. Error: " << GetLastError() << std::endl;
        return 1;
    }

    // Allocate memory in the target process for the DLL path
    LPVOID remoteMemory = VirtualAllocEx(hProcess, NULL, dllPath.size() * sizeof(wchar_t), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteMemory) {
        std::wcerr << L"Failed to allocate memory. Error: " << GetLastError() << std::endl;
        CloseHandle(hProcess);
        return 1;
    }

    // Write the DLL path to the target process
    if (!WriteProcessMemory(hProcess, remoteMemory, dllPath.c_str(), dllPath.size() * sizeof(wchar_t), NULL)) {
        std::wcerr << L"Failed to write memory. Error: " << GetLastError() << std::endl;
        VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return 1;
    }

    // Get the address of LoadLibraryW
    LPVOID loadLibraryAddr = (LPVOID)GetProcAddress(GetModuleHandle(L"kernel32.dll"), "LoadLibraryW");
    if (!loadLibraryAddr) {
        std::wcerr << L"Failed to get LoadLibraryW address. Error: " << GetLastError() << std::endl;
        VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return 1;
    }

    // Create a remote thread to call LoadLibraryW with the DLL path
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)loadLibraryAddr, remoteMemory, 0, NULL);
    if (!hThread) {
        std::wcerr << L"Failed to create remote thread. Error: " << GetLastError() << std::endl;
        VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return 1;
    }

    // Wait for the thread to finish
    WaitForSingleObject(hThread, INFINITE);

    // Clean up
    VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);
    CloseHandle(hThread);
    CloseHandle(hProcess);

    std::wcout << L"DLL injected successfully into " << targetProcess << std::endl;
    return 0;
}
