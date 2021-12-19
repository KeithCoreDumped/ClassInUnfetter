#define _CRT_SECURE_NO_WARNINGS
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include <Windows.h>
#include <iostream>
#include <tlhelp32.h>
#include <Locale.h>
#include <vector>

HWND hMainWindow;
HWND hLineEdit;
HINSTANCE hInstance;
LPCWSTR lpClassName = L"window class";
LPCWSTR lpWindowName = L"ClassInUnfetter";
LPCWSTR lpClassInWindowCaptionPrefix = L"Classroom_";
LPCWSTR lpClassInProcessName = L"ClassIn.exe";
LPCWSTR lpTextNotFound = L"ClassIn教室窗口不存在";
LPCWSTR lpTextRefresh = L"刷新或键入窗口句柄以开始";
LPCWSTR lpStrText = lpTextRefresh;
HANDLE hOutputHandle;
CONST UINT idButton = 233;
CONST UINT idLineEdit = 1233;
CONST UINT idTimer1 = 2233;
CONST UINT idStatic = 3233;
CONST UINT idMenu = 4233;
CONST INT idHotkey = 5233;

const UINT hOffset = 0, vOffset = 0;
UINT nViewWidth = 800, nViewHeight = 600;
UINT nThisWindowPosX = 0, nThisWindowPosY = 0;
UINT nClassInWindowWidth, nClassInWindowHeight, nViewPosX, nViewPosY, nClassInWindowPosX, nClassInWindowPosY;
BOOL bMovedAway = FALSE, bThisTopmost = FALSE, bClassInBottom = FALSE;
// 
HDC hdcClassInWindow = NULL, hdcThisWindow = NULL, hdcThisMem = NULL;
HBITMAP hThisBmp = NULL;
HWND hClassInWindow = NULL;
HMENU hMenuRoot, hMenuClassIn, hMenuUnfetter;

HFONT hFontMSYH32;
std::vector<HWND> vhwnd;

#define AllocDebugConsole() do {                         \
    if(!hOutputHandle) {                                 \
        AllocConsole();                                  \
        hOutputHandle = GetStdHandle(STD_OUTPUT_HANDLE); \
        freopen("CONOUT$", "w+t", stdout);               \
        freopen("CONOUT$", "w+t", stderr);               \
        freopen("CONIN$", "r+t", stdin);                 \
    }                                                    \
} while(0)
#define WriteDebugConsole() std::wcout
#define CheckReleaseDC(_hwnd,_hdc) do{ if((_hdc) != NULL) { ReleaseDC(_hwnd,_hdc); _hdc = NULL; } } while(0)
#define CheckDeleteDC(_hdc) do{ if((_hdc) != NULL) { DeleteDC(_hdc); _hdc = NULL; } } while(0)
#define CheckDeleteObject(_ho) do{ if((_ho) != NULL) { DeleteObject(_ho); _ho = NULL; } } while(0)
#define CheckError() do{ auto _last_error=GetLastError();if(_last_error!=ERROR_SUCCESS) { std::wcout << L"[E] error" << _last_error << L"Line:" << __LINE__ << "\n"; }} while(0)

DWORD GetPIDByName(LPCWSTR pName) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if(INVALID_HANDLE_VALUE == hSnapshot) {
        return NULL;
    }
    PROCESSENTRY32 pe = { sizeof(pe) };
    for(BOOL ret = Process32First(hSnapshot, &pe); ret; ret = Process32Next(hSnapshot, &pe)) {
        if(ua_lstrcmp(pe.szExeFile, pName) == 0) {
            CloseHandle(hSnapshot);
            return pe.th32ProcessID;
        }
    }
    CloseHandle(hSnapshot);
    return 0;
}

void GetAllWindowsFromProcessID(DWORD dwProcessID, std::vector <HWND>& vhWnds) {
    if(dwProcessID == 0)
        return;
    HWND hCurWnd = nullptr;
    do {
        hCurWnd = FindWindowEx(nullptr, hCurWnd, nullptr, nullptr);
        DWORD checkProcessID = 0;
        GetWindowThreadProcessId(hCurWnd, &checkProcessID);
        if(checkProcessID == dwProcessID) {
            vhWnds.push_back(hCurWnd);  // add the found hCurWnd to the vector
            //wprintf(L"Found hWnd %d\n", hCurWnd);
        }
    } while(hCurWnd != nullptr);
}

HWND GetClassroomHWND() {
    // GetPidByName
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    DWORD dwProcessID = 0;
    HWND hCIWnd = NULL;
    DWORD checkProcessID = 0;
    WCHAR lpWindowCaption[16] = { 0 };
    if(INVALID_HANDLE_VALUE == hSnapshot) {
        return NULL;
    }
    PROCESSENTRY32W pe = { sizeof(pe) };
    for(BOOL ret = Process32FirstW(hSnapshot, &pe); ret; ret = Process32NextW(hSnapshot, &pe)) {
        // compare names
        if(wcscmp(pe.szExeFile, lpClassInProcessName) == 0) {
            dwProcessID = pe.th32ProcessID;
        }
    }
    CloseHandle(hSnapshot);
    if(dwProcessID == 0)
        return NULL;
    //GetFirstWindowsFromProcessID
    do {
        hCIWnd = FindWindowExW(nullptr, hCIWnd, nullptr, nullptr);
        GetWindowThreadProcessId(hCIWnd, &checkProcessID);
        if(checkProcessID == dwProcessID) {
            GetWindowTextW(hCIWnd, lpWindowCaption, 16);
            if(wcsncmp(lpWindowCaption, lpClassInWindowCaptionPrefix, wcslen(lpClassInWindowCaptionPrefix)) == 0) {
                return hCIWnd;
            }
        }
    } while(hCIWnd != nullptr);
    return NULL;
}

void RedrawView() {
    if(hClassInWindow) {
        //ClassInWindow valid
        PrintWindow(hClassInWindow, hdcThisMem, 0);
        CheckError();
        BitBlt(hdcThisWindow, hOffset, vOffset, nViewWidth, nViewHeight, hdcThisMem, nViewPosX, nViewPosY, SRCCOPY);
        CheckError();
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    RECT rect{ 0 };
    WCHAR lpStr[32] = { 0 };
    POINT pt{ 0 };
    switch(message) {
    case WM_CREATE:
    {
        // ctrl + F1
        if(!RegisterHotKey(hWnd, idHotkey, MOD_CONTROL, VK_F1))
            WriteDebugConsole() << L"hotkey register failed\n";
        // for chinese character output
        _wsetlocale(LC_ALL, L"chs");
        // font msyh
        auto hFontMSYH20 = CreateFontW(20, 0, 0, 0, 0, FALSE, FALSE, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                       CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"微软雅黑");
        hFontMSYH32 = CreateFontW(32, 0, 0, 0, 0, FALSE, FALSE, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                  CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"微软雅黑");


        // widgets
        hLineEdit = CreateWindowW(L"Edit", NULL, WS_CHILD | WS_BORDER | WS_VISIBLE |
                                  ES_CENTER | ES_AUTOHSCROLL,
                                  20, 130, 100, 26, hWnd, (HMENU)idLineEdit + 1, NULL, NULL);
        SendMessageW(hLineEdit, WM_SETFONT, (WPARAM)hFontMSYH32, TRUE);

        //setup standard scroll bars
        SCROLLINFO si{ 0 };
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_DISABLENOSCROLL;
        SetScrollInfo(hWnd, SB_HORZ, &si, TRUE);
        SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
        //refresh at 60Hz
        SetTimer(hWnd, idTimer1, 1000 / 60, NULL);
    }
    break;//WM_CREATE
    case WM_PAINT:
    {
        PAINTSTRUCT ps{ 0 };
        HDC hdcPaint;
        RedrawView();
        hdcPaint = BeginPaint(hWnd, &ps);
        if(hClassInWindow) {
            //ClassInWindow valid
            PrintWindow(hClassInWindow, hdcThisMem, 0);
            CheckError();
            BitBlt(hdcThisWindow, hOffset, vOffset, nViewWidth, nViewHeight, hdcThisMem, nViewPosX, nViewPosY, SRCCOPY);
            CheckError();
        }
        else {
            GetClientRect(hWnd, &rect);
            nViewWidth = rect.right - rect.left;
            nViewHeight = rect.bottom - rect.top;
            nViewPosX = rect.left;
            nViewPosY = rect.top;
            SelectObject(hdcPaint, hFontMSYH32);
            DrawTextW(hdcPaint, lpStrText, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            MoveWindow(hLineEdit, nViewPosX + nViewWidth / 2 - 100, nViewPosY + nViewHeight / 2 + 10 + 16, 200, 37, TRUE);
        }

        EndPaint(hWnd, &ps);
        return DefWindowProcW(hWnd, message, wParam, lParam);
    }
    break;
    case WM_HOTKEY:
        if((int)wParam == idHotkey) {
            bThisTopmost = !bThisTopmost;
            if(bThisTopmost) {
                CheckMenuItem(hMenuUnfetter, idMenu + 100, MF_BYCOMMAND | MF_CHECKED);
                GetCursorPos(&pt);
                SetWindowPos(hWnd, HWND_TOPMOST, pt.x - nViewWidth / 2, pt.y - 15, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE);
            }
            else {
                CheckMenuItem(hMenuUnfetter, idMenu + 100, MF_BYCOMMAND | MF_UNCHECKED);
                SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOMOVE);
            }
        }
        break;
    case WM_COMMAND:
    {
        switch(LOWORD(wParam)) {
        case idMenu://menu refresh
        {
            //refresh HWND's & Rect
            hClassInWindow = GetClassroomHWND();
            GetWindowTextW(hLineEdit, lpStr, 32);
            HWND hFromUser = NULL;
            if(wcsnlen(lpStr, 32) != 0) {
                swscanf(lpStr, L"%X", &hFromUser);
            }
            if(hClassInWindow == NULL && hFromUser == NULL) {
                wprintf(L"[W] Classroom window not found\n");
                GetClientRect(hWnd, &rect);
                InvalidateRect(hWnd, &rect, TRUE);
                ShowWindow(hLineEdit, SW_SHOW);
                lpStrText = lpTextNotFound;
                break;
            }
            if(hFromUser) {
                hClassInWindow = hFromUser;
                SetWindowTextW(hLineEdit, NULL);
            }

            GetWindowRect(hClassInWindow, &rect);
            nClassInWindowPosX = rect.left;
            nClassInWindowPosY = rect.top;
            nClassInWindowWidth = rect.right - rect.left;
            nClassInWindowHeight = rect.bottom - rect.top;

            GetWindowRect(hWnd, &rect);
            nThisWindowPosX = rect.left;
            nThisWindowPosY = rect.top;

            // create DC & BMP
            CheckReleaseDC(hWnd, hdcThisWindow);
            CheckDeleteDC(hdcThisMem);
            CheckDeleteObject(hThisBmp);
            hdcThisWindow = GetDC(hWnd);
            hdcThisMem = CreateCompatibleDC(hdcThisWindow);

            hThisBmp = CreateCompatibleBitmap(hdcThisWindow, nClassInWindowWidth, nClassInWindowHeight);
            SelectObject(hdcThisMem, hThisBmp);

            ShowWindow(hLineEdit, SW_HIDE);
            RedrawView();
            SCROLLINFO info;
            info.cbSize = sizeof(SCROLLINFO);
            info.fMask = SIF_ALL;
            info.nMin = 0;
            info.nMax = nClassInWindowWidth;
            if(info.nMax < 0)info.nMax = 0;
            info.nPage = nViewWidth;
            info.nPos = 0;
            info.nTrackPos = 0;
            SetScrollInfo(hWnd, SB_HORZ, &info, TRUE);
            info.cbSize = sizeof(SCROLLINFO);
            info.fMask = SIF_ALL;
            info.nMin = 0;
            info.nMax = nClassInWindowHeight;
            if(info.nMax < 0)info.nMax = 0;
            info.nPage = nViewHeight;
            info.nPos = 0;
            info.nTrackPos = 0;
            SetScrollInfo(hWnd, SB_VERT, &info, TRUE);
        }
        break;
        case idMenu + 1://menu hide
        {
            if(!hClassInWindow)
                break;
            bMovedAway = !bMovedAway;
            if(bMovedAway) {
                CheckMenuItem(hMenuClassIn, idMenu + 1, MF_BYCOMMAND | MF_CHECKED);
                MoveWindow(hClassInWindow, 6666, 6666, nClassInWindowWidth, nClassInWindowHeight, FALSE);
            }
            else {
                CheckMenuItem(hMenuClassIn, idMenu + 1, MF_BYCOMMAND | MF_UNCHECKED);
                MoveWindow(hClassInWindow, nClassInWindowPosX, nClassInWindowPosY, nClassInWindowWidth, nClassInWindowHeight, FALSE);
            }
        }
        break;
        case idMenu + 2://menu Bottom most
        {
            if(!hClassInWindow)
                break;
            bClassInBottom = !bClassInBottom;
            if(bClassInBottom) {
                CheckMenuItem(hMenuClassIn, idMenu + 2, MF_BYCOMMAND | MF_CHECKED);
                SetWindowPos(hClassInWindow, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOREDRAW | SWP_NOACTIVATE);
            }
            else {
                SetWindowPos(hClassInWindow, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOREDRAW);
                CheckMenuItem(hMenuClassIn, idMenu + 2, MF_BYCOMMAND | MF_UNCHECKED);
            }
        }
        break;
        case idMenu + 3://menu close
        {
            SendMessageW(hClassInWindow, WM_CLOSE, NULL, NULL);
            hClassInWindow = NULL;
            lpStrText = lpTextRefresh;
            GetClientRect(hWnd, &rect);
            InvalidateRect(hWnd, &rect, TRUE);
            ShowWindow(hLineEdit, SW_SHOW);
        }
        break;
        case idMenu + 4://menu check
        {
            AllocDebugConsole();
            vhwnd.clear();
            GetAllWindowsFromProcessID(GetPIDByName(lpClassInProcessName), vhwnd);
            std::wcout << L"\n";
            for(auto iter = vhwnd.begin(); iter != vhwnd.end(); iter++) {
                GetWindowTextW(*iter, lpStr, 32);
                std::wcout << L"HWND:" << *iter << L" Caption" << lpStr << L"\n";
            }
        }
        break;
        case idMenu + 100://menu topmost
        {
            bThisTopmost = !bThisTopmost;
            if(bThisTopmost) {
                CheckMenuItem(hMenuUnfetter, idMenu + 100, MF_BYCOMMAND | MF_CHECKED);
                SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOMOVE);
            }
            else {
                CheckMenuItem(hMenuUnfetter, idMenu + 100, MF_BYCOMMAND | MF_UNCHECKED);
                SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOMOVE);
            }
            break;
        default:
            return DefWindowProcW(hWnd, message, wParam, lParam);
            break;
        }
        }
    }
    break;//WM_COMMAND
    case WM_TIMER:
        if(!hClassInWindow)
            break;
        GetClientRect(hWnd, &rect);
        InvalidateRect(hWnd, &rect, false);
        break;
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_SETCURSOR:
        if(hClassInWindow)
            SendMessageW(hClassInWindow, message, wParam, MAKELPARAM(
                LOWORD(lParam) + nViewPosX,
                HIWORD(lParam) + nViewPosY
            ));
        return DefWindowProcW(hWnd, message, wParam, lParam);
        break;
    case WM_CUT:
    case WM_COPY:
    case WM_PASTE:
    case WM_CLEAR:
    case WM_UNDO:
        if(hClassInWindow)
            SendMessageW(hClassInWindow, message, wParam, lParam);
        return DefWindowProcW(hWnd, message, wParam, lParam);
        break;
    case WM_MOUSEWHEEL:
        if(hClassInWindow) {
            if(bMovedAway) {
                SendMessageW(hClassInWindow, message, wParam, MAKELPARAM(
                    LOWORD(lParam) + 6666 - nThisWindowPosX + nViewPosX,
                    HIWORD(lParam) + 6666 - nThisWindowPosY + nViewPosY
                ));
            }
            else {
                SendMessageW(hClassInWindow, message, wParam, MAKELPARAM(
                    LOWORD(lParam) + nClassInWindowPosX - nThisWindowPosX + nViewPosX,
                    HIWORD(lParam) + nClassInWindowPosY - nThisWindowPosY + nViewPosY
                ));
            }
        }
        return DefWindowProcW(hWnd, message, wParam, lParam);
        break;
    case WM_NCHITTEST:
    case WM_NCMOUSEMOVE:
        //point translation required
        pt = *(POINT*)&lParam;
        pt.x += nClassInWindowPosX - nThisWindowPosX;
        pt.y += nClassInWindowPosY - nThisWindowPosY;
        if(hClassInWindow)
            SendMessageW(hClassInWindow, message, wParam, lParam);
        return DefWindowProcW(hWnd, message, wParam, lParam);
        break;
    //case WM_MOUSEHOVER:
    //case WM_MOUSELEAVE:
    /*
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONUP:
    case WM_NCLBUTTONDBLCLK:
    case WM_NCRBUTTONDOWN:
    case WM_NCRBUTTONUP:
    case WM_NCRBUTTONDBLCLK:
    case WM_NCMBUTTONDOWN:
    case WM_NCMBUTTONUP:
    */
    case WM_SIZE:
    {
        nViewWidth = LOWORD(lParam);
        nViewHeight = HIWORD(lParam);
        if(!hClassInWindow)
            break;
        CheckReleaseDC(hWnd, hdcThisWindow);
        CheckDeleteDC(hdcThisMem);
        CheckDeleteObject(hThisBmp);
        hdcThisWindow = GetDC(hWnd);
        hdcThisMem = CreateCompatibleDC(hdcThisWindow);
        hThisBmp = CreateCompatibleBitmap(hdcThisWindow, nClassInWindowWidth, nClassInWindowHeight);
        SelectObject(hdcThisMem, hThisBmp);
        RedrawView();
        SCROLLINFO info;
        info.cbSize = sizeof(SCROLLINFO);
        info.fMask = SIF_ALL;
        info.nMin = 0;
        info.nMax = nClassInWindowWidth;
        info.nPage = nViewWidth;
        info.nPos = 0;
        info.nTrackPos = 0;
        SetScrollInfo(hWnd, SB_HORZ, &info, TRUE);
        info.cbSize = sizeof(SCROLLINFO);
        info.fMask = SIF_ALL;
        info.nMin = 0;
        info.nMax = nClassInWindowHeight;
        info.nPage = nViewHeight;
        info.nPos = 0;
        info.nTrackPos = 0;
        SetScrollInfo(hWnd, SB_VERT, &info, TRUE);
        nViewPosX = 0, nViewPosY = 0;
        GetClientRect(hWnd, &rect);
        InvalidateRect(hWnd, &rect, false);
    }
    break;
    case WM_MOVE:
        nThisWindowPosX = LOWORD(lParam);
        nThisWindowPosY = HIWORD(lParam);
        break;
    case WM_HSCROLL:
    {
        switch(LOWORD(wParam)) {
        case SB_THUMBPOSITION:
        case SB_THUMBTRACK:
            nViewPosX = HIWORD(wParam);
            SetScrollPos(hWnd, SB_HORZ, nViewPosX, TRUE);
            RedrawView();
            break;
        case SB_LINELEFT:
            nViewPosX -= 20;
            if(nViewPosX < 0)nViewPosX = 0;
            SetScrollPos(hWnd, SB_HORZ, nViewPosX, TRUE);
            RedrawView();
            break;
        case SB_LINERIGHT:
            nViewPosX += 20;
            if(nViewPosX > nClassInWindowWidth - nViewWidth)nViewPosX = nClassInWindowWidth - nViewWidth;
            SetScrollPos(hWnd, SB_HORZ, nViewPosX, TRUE);
            RedrawView();
            break;
        default:
            break;
        }
    }
    break;//WM_HSCROLL
    case WM_VSCROLL:
    {
        switch(LOWORD(wParam)) {
        case SB_THUMBPOSITION:
        case SB_THUMBTRACK:
            SetScrollPos(hWnd, SB_VERT, HIWORD(wParam), TRUE);
            nViewPosY = HIWORD(wParam);
            RedrawView();
            break;
        case SB_LINELEFT:
            nViewPosY -= 20;
            if(nViewPosY < 0)nViewPosY = 0;
            SetScrollPos(hWnd, SB_VERT, nViewPosY, TRUE);
            RedrawView();
            break;
        case SB_LINERIGHT:
            nViewPosY += 20;
            if(nViewPosY > nClassInWindowHeight - nViewHeight)nViewPosY = nClassInWindowHeight - nViewHeight;
            SetScrollPos(hWnd, SB_VERT, nViewPosY, TRUE);
            RedrawView();
            break;
        default:
            break;
        }
    }
    break;//WM_VSCROLL
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_CLOSE:
        UnregisterHotKey(hWnd, idHotkey);
        if(bMovedAway) {
            CheckMenuItem(hMenuClassIn, idMenu + 1, MF_BYCOMMAND | MF_UNCHECKED);
            MoveWindow(hClassInWindow, nClassInWindowPosX, nClassInWindowPosY, nClassInWindowWidth, nClassInWindowHeight, FALSE);
        }
        return DefWindowProcW(hWnd, message, wParam, lParam);
        break;
    default:
        return DefWindowProcW(hWnd, message, wParam, lParam);
    }
    return 0;
}

int APIENTRY wWinMain(_In_ HINSTANCE _hInstance,
                      _In_opt_ HINSTANCE _hPrevInstance,
                      _In_ LPWSTR    _lpCmdLine,
                      _In_ int       _nCmdShow) {
    UNREFERENCED_PARAMETER(_hPrevInstance);
    UNREFERENCED_PARAMETER(_lpCmdLine);
    hInstance = _hInstance;
    //register window class
    WNDCLASSEXW wcex{ 0 };
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = lpClassName;
    wcex.lpszMenuName = NULL;
    wcex.hIconSm = LoadIconW(NULL, IDI_APPLICATION);
    RegisterClassExW(&wcex);
    // create menu
    hMenuRoot = CreateMenu();
    hMenuClassIn = CreatePopupMenu();
    hMenuUnfetter = CreatePopupMenu();
    AppendMenuW(hMenuRoot, MF_POPUP, (UINT_PTR)hMenuClassIn, L"ClassIn");
    AppendMenuW(hMenuRoot, MF_POPUP, (UINT_PTR)hMenuUnfetter, L"Unfetter");
    AppendMenuW(hMenuClassIn, MF_STRING, idMenu, L"刷新");
    AppendMenuW(hMenuClassIn, MF_STRING, idMenu + 1, L"隐藏");
    AppendMenuW(hMenuClassIn, MF_STRING, idMenu + 2, L"置底");
    AppendMenuW(hMenuClassIn, MF_STRING, idMenu + 3, L"关闭");
    AppendMenuW(hMenuClassIn, MF_STRING, idMenu + 4, L"Check");
    AppendMenuW(hMenuUnfetter, MF_STRING, idMenu + 100, L"置顶");

    hMainWindow = CreateWindowW(lpClassName, lpWindowName, WS_OVERLAPPEDWINDOW | WS_HSCROLL | WS_VSCROLL,
                                CW_USEDEFAULT, 0, 800, 600, nullptr, hMenuRoot, hInstance, nullptr);
    if(!hMainWindow) {
        MessageBoxW(hMainWindow, L"Failed to Create Main Window", L"Error", MB_ICONERROR | MB_OK);
        return 1;
    }
    ShowWindow(hMainWindow, _nCmdShow);
    UpdateWindow(hMainWindow);

    MSG msg;

    // 主消息循环:
    while(GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
