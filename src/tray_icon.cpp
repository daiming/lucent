/**
 * 系统托盘图标
 * 在任务栏通知区域显示当前日期数字图标
 * 支持动态图标生成和右键菜单
 */

#include <windows.h>
#include <algorithm>
#include "tray_icon.h"

bool TrayIcon::Create(HWND hOwner, UINT callbackMsg) {
    ZeroMemory(&nid_, sizeof(nid_));
    nid_.cbSize = sizeof(NOTIFYICONDATAW);
    nid_.hWnd = hOwner;
    nid_.uID = 1;
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid_.uCallbackMessage = callbackMsg;

    // 先用默认应用图标
    nid_.hIcon = LoadIcon(nullptr, IDI_APPLICATION);

    if (!Shell_NotifyIconW(NIM_ADD, &nid_)) {
        return false;
    }
    created_ = true;
    return true;
}

void TrayIcon::Remove() {
    if (created_) {
        Shell_NotifyIconW(NIM_DELETE, &nid_);
        created_ = false;
    }
}

void TrayIcon::UpdateIcon(HICON hIcon) {
    if (!created_) return;
    nid_.hIcon = hIcon;
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
}

void TrayIcon::UpdateTooltip(const std::wstring& text) {
    if (!created_) return;
    // 提示文本最多 128 个字符（含 null）
    size_t copy_len = (std::min)(text.size(), static_cast<size_t>(127));
    wmemcpy(nid_.szTip, text.c_str(), copy_len);
    nid_.szTip[copy_len] = L'\0';
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
}

HICON TrayIcon::GenerateDateIcon(int day, int dpi) {
    // 获取系统小图标尺寸
    int size = GetSystemMetrics(SM_CXSMICON);
    if (size <= 0) size = 16;

    // 创建 32 位 ARGB 位图
    HDC hdcScreen = GetDC(nullptr);
    HDC hdc = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = size;
    bmi.bmiHeader.biHeight = -size;  // 自上而下
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
    HBITMAP hOldBitmap = static_cast<HBITMAP>(SelectObject(hdc, hBitmap));

    // 填充透明背景
    DWORD* pixels = static_cast<DWORD*>(pBits);
    for (int i = 0; i < size * size; i++) {
        pixels[i] = 0x00000000;  // 完全透明
    }

    // 绘制圆形背景
    HBRUSH hBrush = CreateSolidBrush(RGB(0, 120, 215));  // 蓝色圆圈
    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(0, 120, 215));
    HBRUSH hOldBrush = static_cast<HBRUSH>(SelectObject(hdc, hBrush));
    HPEN hOldPen = static_cast<HPEN>(SelectObject(hdc, hPen));

    int margin = 1;
    // 手动填充圆形区域（因为 GDI 不直接支持透明背景的椭圆填充）
    int cx = size / 2;
    int cy = size / 2;
    int r = size / 2 - margin;
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int dx = x - cx;
            int dy = y - cy;
            if (dx * dx + dy * dy <= r * r) {
                pixels[y * size + x] = 0xFFDC7800;  // ARGB: 蓝色 (ABGR 格式)
            }
        }
    }

    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);
    DeleteObject(hBrush);
    DeleteObject(hPen);

    // 绘制日期数字
    wchar_t dayText[4];
    swprintf_s(dayText, L"%d", day);

    // 根据图标大小选择合适的字体
    int fontSize = MulDiv(8, dpi, 96);
    if (fontSize < 8) fontSize = 8;
    HFONT hFont = CreateFontW(
        -fontSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI");
    HFONT hOldFont = static_cast<HFONT>(SelectObject(hdc, hFont));

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));

    RECT rc = { 0, 0, size, size };
    DrawTextW(hdc, dayText, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // 恢复并清理 GDI 对象
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);

    SelectObject(hdc, hOldBitmap);
    DeleteDC(hdc);
    ReleaseDC(nullptr, hdcScreen);

    // 创建掩码位图
    HDC hdcMask = CreateCompatibleDC(nullptr);
    HBITMAP hMask = CreateBitmap(size, size, 1, 1, nullptr);
    HBITMAP hOldMask = static_cast<HBITMAP>(SelectObject(hdcMask, hMask));

    // 从颜色位图生成掩码：透明像素为白色，非透明为黑色
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            DWORD alpha = (pixels[y * size + x] >> 24) & 0xFF;
            SetPixel(hdcMask, x, y, alpha > 0 ? RGB(0, 0, 0) : RGB(255, 255, 255));
        }
    }
    SelectObject(hdcMask, hOldMask);
    DeleteDC(hdcMask);

    // 从位图创建图标
    ICONINFO ii = {};
    ii.fIcon = TRUE;
    ii.xHotspot = 0;
    ii.yHotspot = 0;
    ii.hbmMask = hMask;
    ii.hbmColor = hBitmap;

    HICON hIcon = CreateIconIndirect(&ii);

    DeleteObject(hBitmap);
    DeleteObject(hMask);

    return hIcon;
}
