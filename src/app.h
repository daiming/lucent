#pragma once

#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <shlwapi.h>

#include "config.h"
#include "resource.h"
#include "tray_icon.h"
#include "calendar_window.h"
#include "holiday_service.h"

// CApp - 应用生命周期管理（单例）
class CApp {
public:
    CApp() = default;
    ~CApp() { Quit(); }

    // 初始化应用：互斥锁、COM、托盘、假日数据、日历窗口
    bool Init(HINSTANCE hInst) {
        hInst_ = hInst;

        // 单实例检测
        mutex_ = CreateMutexW(nullptr, FALSE, cfg::kMutexName);
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            // 尝试激活已存在的实例窗口
            HWND existing = FindWindowW(L"LucentCalendar", nullptr);
            if (existing) {
                ShowWindow(existing, SW_SHOW);
                SetForegroundWindow(existing);
            }
            return false;
        }

        // 初始化 COM（托盘图标通知需要）
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

        // 初始化 GDI+（抗锯齿圆角绘制需要）
        CalendarRenderer::InitGdiPlus();

        // 初始化公共控件
        INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_STANDARD_CLASSES };
        InitCommonControlsEx(&icex);

        // 确保数据目录存在
        cfg::EnsureDataDirectory();

        // 加载假日数据（后台异步补充近3年）
        holidays_.Load();
        holidays_.EnsureRecentYears(GetCurrentYear());

        // 注册并创建消息窗口（用于接收托盘回调）
        WNDCLASSW wc = {};
        wc.lpfnWndProc = MsgWndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = L"LucentMsgWindow";
        RegisterClassW(&wc);
        msg_hwnd_ = CreateWindowW(L"LucentMsgWindow", L"", 0,
                                  0, 0, 0, 0,
                                  HWND_MESSAGE, nullptr, hInst, this);

        // 创建托盘图标
        tray_.Create(msg_hwnd_, WM_TRAYICON);
        HICON hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_TRAY));
        tray_.UpdateIcon(hIcon);
        tray_.UpdateTooltip(BuildTooltipText());

        // 日历窗口延迟创建：首次 Show() 时按需创建，确保 DPI 和尺寸正确
        calendar_.SetParams(hInst, &holidays_);

        return true;
    }

    // 消息循环
    int Run() {
        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        return static_cast<int>(msg.wParam);
    }

    void Quit() {
        if (!mutex_) return;

        // 保存假日数据
        holidays_.Save();

        // 移除托盘图标
        tray_.Remove();

        // 销毁窗口
        if (msg_hwnd_) DestroyWindow(msg_hwnd_);

        CalendarRenderer::ShutdownGdiPlus();
        CoUninitialize();

        if (mutex_) {
            CloseHandle(mutex_);
            mutex_ = nullptr;
        }
    }

private:
    HINSTANCE hInst_ = nullptr;
    HANDLE mutex_ = nullptr;
    HWND msg_hwnd_ = nullptr;

    TrayIcon tray_;
    HolidayService holidays_;
    CalendarWindow calendar_;

    static int GetCurrentYear() {
        SYSTEMTIME st;
        GetLocalTime(&st);
        return st.wYear;
    }

    static int GetCurrentDay() {
        SYSTEMTIME st;
        GetLocalTime(&st);
        return st.wDay;
    }

    std::wstring BuildTooltipText() {
        SYSTEMTIME st;
        GetLocalTime(&st);
        wchar_t buf[128];
        swprintf_s(buf, L"%s %04d-%02d-%02d", cfg::kAppName, st.wYear, st.wMonth, st.wDay);
        return buf;
    }

    // 隐藏消息窗口的窗口过程（处理托盘图标回调）
    static LRESULT CALLBACK MsgWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        auto* self = reinterpret_cast<CApp*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        if (msg == WM_NCCREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            self = reinterpret_cast<CApp*>(cs->lpCreateParams);
        }
        if (!self) return DefWindowProc(hwnd, msg, wp, lp);

        if (msg == WM_TRAYICON) {
            return self->OnTrayIcon(hwnd, wp, lp);
        }
        if (msg == WM_COMMAND) {
            return self->OnCommand(wp, lp);
        }

        return DefWindowProc(hwnd, msg, wp, lp);
    }

    LRESULT OnTrayIcon(HWND hwnd, WPARAM wp, LPARAM lp) {
        if (LOWORD(lp) == WM_LBUTTONUP) {
            // 左键点击托盘图标：显示/隐藏日历
            calendar_.ToggleShow(hwnd);
        } else if (LOWORD(lp) == WM_RBUTTONUP) {
            // 右键点击：弹出上下文菜单
            ShowContextMenu(hwnd);
        }
        return 0;
    }

    LRESULT OnCommand(WPARAM wp, LPARAM lp) {
        switch (LOWORD(wp)) {
        case IDM_EXIT:
            PostQuitMessage(0);
            break;
        case IDM_TODAY:
            calendar_.Show(msg_hwnd_);
            break;
        case IDM_AUTOSTART:
            ToggleAutoStart();
            break;
        case IDM_ABOUT: {
            std::wstring about_title = std::wstring(L"关于 ") + cfg::kAppName;
            std::wstring about_text =
                std::wstring(cfg::kAppName) + L" v" + cfg::kAppVersion + L" - " + cfg::kAppDesc +
                L"\n\n" + cfg::kAppFeatures +
                L"\n\n" + cfg::kAppUrl;
            MessageBoxW(nullptr, about_text.c_str(), about_title.c_str(),
                        MB_OK | MB_ICONINFORMATION);
            break;
        }
        }
        return 0;
    }

    void ShowContextMenu(HWND hwnd) {
        POINT pt;
        GetCursorPos(&pt);
        HMENU hMenu = CreatePopupMenu();
        AppendMenuW(hMenu, MF_STRING, IDM_TODAY, L"显示界面");

        UINT flags = MF_STRING;
        if (IsAutoStartEnabled()) flags |= MF_CHECKED;
        AppendMenuW(hMenu, flags, IDM_AUTOSTART, L"开机启动");

        AppendMenuW(hMenu, MF_STRING, IDM_ABOUT,
                    (std::wstring(L"关于 ") + cfg::kAppName).c_str());
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"退出");

        SetForegroundWindow(hwnd);
        TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN,
                       pt.x, pt.y, 0, hwnd, nullptr);
        DestroyMenu(hMenu);
    }

    // 开机启动：通过注册表 HKCU\Software\Microsoft\Windows\CurrentVersion\Run
    static bool IsAutoStartEnabled() {
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER,
                L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                0, KEY_READ, &hKey) != ERROR_SUCCESS) return false;
        wchar_t value[MAX_PATH] = {};
        DWORD size = sizeof(value);
        LSTATUS status = RegQueryValueExW(hKey, cfg::kAppName, nullptr, nullptr,
                                           reinterpret_cast<LPBYTE>(value), &size);
        RegCloseKey(hKey);
        if (status != ERROR_SUCCESS) return false;
        // 验证路径是否指向当前 exe
        wchar_t exe_path[MAX_PATH];
        GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
        return _wcsicmp(value, exe_path) == 0;
    }

    static void ToggleAutoStart() {
        if (IsAutoStartEnabled()) {
            // 取消开机启动
            HKEY hKey;
            if (RegOpenKeyExW(HKEY_CURRENT_USER,
                    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                    0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
                RegDeleteValueW(hKey, cfg::kAppName);
                RegCloseKey(hKey);
            }
        } else {
            // 设置开机启动
            HKEY hKey;
            if (RegOpenKeyExW(HKEY_CURRENT_USER,
                    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                    0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
                wchar_t exe_path[MAX_PATH];
                GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
                RegSetValueExW(hKey, cfg::kAppName, 0, REG_SZ,
                               reinterpret_cast<const BYTE*>(exe_path),
                               static_cast<DWORD>((wcslen(exe_path) + 1) * sizeof(wchar_t)));
                RegCloseKey(hKey);
            }
        }
    }
};
