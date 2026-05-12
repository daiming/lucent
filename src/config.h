#pragma once

#include <windows.h>
#include <shlobj.h>
#include <string>

namespace cfg {

constexpr const wchar_t* kAppName = L"微光日历";
constexpr const wchar_t* kAppVersion = L"1.0.0";
constexpr const wchar_t* kAppDesc = L"极简日历应用";
constexpr const wchar_t* kAppFeatures = L"公历 · 农历 · 节假日 · 调休补班";
constexpr const wchar_t* kAppUrl = L"https://github.com/daiming/lucent";
constexpr const wchar_t* kMutexName = L"LucentSingletonMutex";
constexpr const wchar_t* kAppDataSubdir = L"Lucent";
constexpr const wchar_t* kHolidaysFile = L"\\data\\holidays.json";
constexpr const wchar_t* kConfigFile = L"\\config.json";
constexpr const wchar_t* kApiHost = L"timor.tech";
constexpr const wchar_t* kApiPathPrefix = L"/api/holiday/year/";

// 窗口基础尺寸（96 DPI 基准）- 4:3 比例左右分栏布局
constexpr int kBaseWidth = 640;      // 4:3 比例
constexpr int kBaseHeight = 480;
constexpr int kLeftPanelWidth = 170; // 左侧面板宽度
constexpr int kTitleBarHeight = 59;     // 标题栏
constexpr int kWeekdayHeaderHeight = 30;
constexpr int kCellHeight = 58;
constexpr int kFooterHeight = 0;
constexpr int kMargin = 10;
constexpr int kShadowSize = 8;    // 窗口外围阴影宽度

// 颜色
constexpr COLORREF kColorHoliday = RGB(220, 50, 50);       // 忌标签、节日名
constexpr COLORREF kColorHolidayBg = RGB(241, 255, 238);   // #f1ffee 休息日背景
constexpr COLORREF kColorHolidayText = RGB(0, 140, 140);     // #008c8c 调休文字
constexpr COLORREF kColorRestBadge = RGB(0, 140, 140);      // #008c8c "休" 徽章
constexpr COLORREF kColorWorkBg = RGB(255, 242, 234);       // #fff2ea 补班背景
constexpr COLORREF kColorWorkday = RGB(238, 131, 162);       // #ee83a2 补班文字
constexpr COLORREF kColorWorkBadge = RGB(232, 88, 39);       // #E85827 "班" 徽章
constexpr COLORREF kColorToday = RGB(15, 75, 129);            // #0f4b81 今日文字
constexpr COLORREF kColorTodayBg = RGB(238, 245, 255);       // #eef5ff 今日背景
constexpr COLORREF kColorSelectedBorder = RGB(4, 98, 194);  // #0462c2 选中边框
constexpr COLORREF kColorNormal = RGB(50, 50, 50);
constexpr COLORREF kColorLunar = RGB(140, 140, 140);
constexpr COLORREF kColorOtherMonth = RGB(200, 200, 200);
constexpr COLORREF kColorDivider = RGB(230, 230, 230);
constexpr COLORREF kColorLeftBg = RGB(245, 248, 252);      // 左侧面板背景
constexpr COLORREF kColorLabel = RGB(160, 160, 160);

inline std::wstring GetAppDataPath() {
    wchar_t path[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, path))) {
        return std::wstring(path) + L"\\" + kAppDataSubdir;
    }
    return L"";
}

inline bool EnsureDataDirectory() {
    std::wstring base = GetAppDataPath();
    if (base.empty()) return false;
    std::wstring data_dir = base + L"\\data";
    auto create_dir = [](const std::wstring& dir) -> bool {
        return CreateDirectoryW(dir.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
    };
    create_dir(base);
    return create_dir(data_dir);
}

inline int GetDPI(HWND hwnd) {
    static auto fn = reinterpret_cast<UINT(WINAPI*)(HWND)>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
    if (fn) {
        int dpi = static_cast<int>(fn(hwnd));
        return dpi > 0 ? dpi : 96;
    }
    HDC hdc = GetDC(nullptr);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(nullptr, hdc);
    return dpi > 0 ? dpi : 96;
}

inline int ScaleForDPI(int value, int dpi) {
    return MulDiv(value, dpi, 96);
}

inline int FontHeightForDPI(int pt, int dpi) {
    return -MulDiv(pt, dpi, 72);
}

}  // namespace cfg
