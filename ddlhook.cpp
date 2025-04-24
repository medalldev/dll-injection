#include <windows.h>
#include <fstream>
#include <string>

// Original window procedure
WNDPROC g_originalWndProc = NULL;

// Function to save data to a file
void SaveReportToFile(const std::wstring& data) {
    std::wofstream outFile(L"C:\\Temp\\CapturedReport.txt", std::ios::app);
    if (outFile.is_open()) {
        outFile << L"Report captured at " << std::wstring(CTime::GetCurrentTime().Format(L"%Y-%m-%d %H:%M:%S")) << L":\n";
        outFile << data << L"\n\n";
        outFile.close();
    }
}

// Function to capture report data (simplified example)
std::wstring CaptureReportData(HWND hWnd) {
    // Try to get text from the main window or its child controls
    std::wstring reportData;
    char buffer[4096] = { 0 };
    if (SendMessageA(hWnd, WM_GETTEXT, sizeof(buffer), (LPARAM)buffer)) {
        reportData = std::wstring(buffer, buffer + strlen(buffer));
    } else {
        // Fallback: Check clipboard for report data (some apps copy report content)
        if (OpenClipboard(hWnd)) {
            HANDLE hData = GetClipboardData(CF_TEXT);
            if (hData) {
                const char* clipData = (const char*)GlobalLock(hData);
                if (clipData) {
                    reportData = std::wstring(clipData, clipData + strlen(clipData));
                    GlobalUnlock(hData);
                }
            }
            CloseClipboard();
        }
    }
    return reportData.empty() ? L"No data captured" : reportData;
}

// Custom window procedure to intercept messages
LRESULT CALLBACK HookedWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_COMMAND) {
        // Replace 100 with the actual print menu ID (use Spy++ to find it)
        if (LOWORD(wParam) == 100) {
            // Capture the report data
            std::wstring reportData = CaptureReportData(hwnd);
            // Save or send the data
            SaveReportToFile(reportData);
            // Notify user (optional, for debugging)
            MessageBox(NULL, L"Print action intercepted and report captured!", L"Hook", MB_OK | MB_ICONINFORMATION);
            // Allow the original print action to proceed
            return CallWindowProc(g_originalWndProc, hwnd, msg, wParam, lParam);
        }
    }
    // Call the original window procedure for unhandled messages
    return CallWindowProc(g_originalWndProc, hwnd, msg, wParam, lParam);
}

// Thread to set up the hook
DWORD WINAPI HookThread(LPVOID lpParam) {
    // Find the target window (replace "Notepad" with the target window class or title)
    HWND hTargetWnd = FindWindow(L"Notepad", NULL);
    if (!hTargetWnd) {
        MessageBox(NULL, L"Target window not found!", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Subclass the window to intercept messages
    g_originalWndProc = (WNDPROC)SetWindowLongPtr(hTargetWnd, GWLP_WNDPROC, (LONG_PTR)HookedWndProc);
    if (!g_originalWndProc) {
        MessageBox(NULL, L"Failed to subclass window!", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Keep the thread alive
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

// DLL entry point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        // Create a thread to set up the hook
        CreateThread(NULL, 0, HookThread, NULL, 0, NULL);
        break;
    case DLL_PROCESS_DETACH:
        // Restore the original window procedure
        if (g_originalWndProc) {
            HWND hTargetWnd = FindWindow(L"Notepad", NULL);
            if (hTargetWnd) {
                SetWindowLongPtr(hTargetWnd, GWLP_WNDPROC, (LONG_PTR)g_originalWndProc);
            }
        }
        break;
    }
    return TRUE;
}
