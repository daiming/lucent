/**
 * 日历弹出窗口
 * 无边框、阴影、拖拽支持、DPI 缩放
 *
 * 显隐控制：
 *   visible_ 记录用户意图（true=展示, false=隐藏）
 *   Show()   → visible_ = true  + 创建/显示窗口
 *   Hide()   → visible_ = false + 销毁窗口
 *   ToggleShow() → 根据 visible_ 切换
 *   OnActivate(WA_INACTIVE) → 立即视觉隐藏 + 延迟销毁定时器
 *     定时器确保 ToggleShow 有时间取消，避免消息竞争
 */

#include "calendar_window.h"
#include "resource.h"

#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <algorithm>

#pragma comment(lib, "dwmapi.lib")

bool CalendarWindow::class_registered_ = false;
HBRUSH CalendarWindow::bg_brush_ = nullptr;

CalendarWindow::~CalendarWindow() {
    if (hwnd_ && IsWindow(hwnd_)) DestroyWindow(hwnd_);
}

void CalendarWindow::SetParams(HINSTANCE hInst, HolidayService* holidays) {
    hInst_ = hInst;
    holidays_ = holidays;
}

void CalendarWindow::RegisterClass(HINSTANCE hInst) {
    if (class_registered_) return;
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    bg_brush_ = CreateSolidBrush(RGB(0, 0, 0));
    wc.hbrBackground = bg_brush_;
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);
    class_registered_ = true;
}

bool CalendarWindow::Create(HINSTANCE hInst, HolidayService* holidays) {
    hInst_ = hInst;
    holidays_ = holidays;
    RegisterClass(hInst);

    SYSTEMTIME st;
    GetLocalTime(&st);
    display_year_ = st.wYear;
    display_month_ = st.wMonth;

    int width = cfg::kBaseWidth;
    int height = cfg::kBaseHeight;

    hwnd_ = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        kClassName, L"Lucent Calendar",
        WS_POPUP,
        0, 0, width, height,
        nullptr, nullptr, hInst, this);

    if (!hwnd_) return false;
    return true;
}

void CalendarWindow::Show(HWND hRefWnd) {
    visible_ = true;
    if (hwnd_) KillTimer(hwnd_, kAutoHideTimerId);

    // 窗口已销毁则重建
    if (!hwnd_) {
        CalendarRenderer::InitGdiPlus();
        Create(hInst_, holidays_);
        if (!hwnd_) return;
    }

    // 先确定 DPI 和窗口尺寸，再填充数据
    current_dpi_ = cfg::GetDPI(hwnd_);
    if (current_dpi_ <= 0) current_dpi_ = 96;

    int width = cfg::ScaleForDPI(cfg::kBaseWidth, current_dpi_);
    int height = cfg::ScaleForDPI(cfg::kBaseHeight, current_dpi_);

    RECT work_area;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0);

    int x = work_area.right - width - cfg::ScaleForDPI(12, current_dpi_);
    int y = work_area.bottom - height - cfg::ScaleForDPI(4, current_dpi_);

    // 先调整窗口到正确尺寸，再填充数据并绘制
    SetWindowPos(hwnd_, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE);

    GoToday();

    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    InvalidateRect(hwnd_, nullptr, TRUE);
    UpdateWindow(hwnd_);
    SetForegroundWindow(hwnd_);
}

void CalendarWindow::Hide() {
    visible_ = false;
    if (!hwnd_) return;
    KillTimer(hwnd_, kAutoHideTimerId);
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
    CalendarRenderer::ShutdownGdiPlus();
}

void CalendarWindow::ToggleShow(HWND hRefWnd) {
    if (hwnd_) KillTimer(hwnd_, kAutoHideTimerId);
    if (visible_) Hide();
    else Show(hRefWnd);
}

void CalendarWindow::GoToday() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    display_year_ = st.wYear;
    display_month_ = st.wMonth;
    UpdateModel();

    // 重置选中到今天
    for (int i = 0; i < 42; i++) {
        int row = i / 7, col = i % 7;
        if (model_.GetCell(row, col).is_today) {
            renderer_.SetSelectedCell(i, st.wYear, st.wMonth, st.wDay);
            break;
        }
    }
}

void CalendarWindow::UpdateModel() {
    if (holidays_) model_.Populate(display_year_, display_month_, *holidays_);
}

void CalendarWindow::NavigateMonth(int delta) {
    display_month_ += delta;
    while (display_month_ > 12) { display_month_ -= 12; display_year_++; }
    while (display_month_ < 1) { display_month_ += 12; display_year_--; }
    UpdateModel();

    // 选中日期的年月日保持不变，在当前视图中找到对应格子则高亮
    int sel_y = renderer_.GetSelectedYear();
    int sel_m = renderer_.GetSelectedMonth();
    int sel_d = renderer_.GetSelectedDay();
    if (sel_d > 0) {
        for (int i = 0; i < 42; i++) {
            auto& c = model_.GetCell(i / 7, i % 7);
            if (c.solar_year == sel_y && c.solar_month == sel_m && c.solar_day == sel_d) {
                renderer_.SetSelectedCell(i, sel_y, sel_m, sel_d);
                break;
            }
        }
    }

    if (hwnd_) { InvalidateRect(hwnd_, nullptr, TRUE); UpdateWindow(hwnd_); }
}

void CalendarWindow::ApplyShadow(HWND hwnd) {
    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    // Win11 圆角偏好
    static auto fnDwmSet = reinterpret_cast<HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD)>(
        GetProcAddress(GetModuleHandleW(L"dwmapi.dll"), "DwmSetWindowAttribute"));
    if (fnDwmSet) {
        DWORD corner = 2;  // DWMWCP_ROUND = 2
        fnDwmSet(hwnd, 33, &corner, sizeof(corner));
    }
}

LRESULT CALLBACK CalendarWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    CalendarWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
        self = reinterpret_cast<CalendarWindow*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<CalendarWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    if (!self) return DefWindowProc(hwnd, msg, wp, lp);

    switch (msg) {
    case WM_CREATE:       return self->OnCreate(hwnd, msg, wp, lp);
    case WM_PAINT:        return self->OnPaint(hwnd, msg, wp, lp);
    case WM_ERASEBKGND:   return self->OnEraseBkgnd(hwnd, msg, wp, lp);
    case WM_NCCALCSIZE:   return 0;
    case WM_NCHITTEST:    return self->OnNcHitTest(hwnd, msg, wp, lp);
    case WM_LBUTTONDOWN:  return self->OnLButtonDown(hwnd, msg, wp, lp);
    case WM_ACTIVATE:     return self->OnActivate(hwnd, msg, wp, lp);
    case WM_TIMER:        return self->OnTimer(hwnd, msg, wp, lp);
    case WM_KEYDOWN:      return self->OnKeyDown(hwnd, msg, wp, lp);
    case WM_DPICHANGED:   return self->OnDpiChanged(hwnd, msg, wp, lp);
    case WM_DESTROY:      return self->OnDestroy(hwnd, msg, wp, lp);
    default:              return DefWindowProc(hwnd, msg, wp, lp);
    }
}

LRESULT CalendarWindow::OnCreate(HWND hwnd, UINT, WPARAM, LPARAM) {
    current_dpi_ = cfg::GetDPI(hwnd);
    if (current_dpi_ <= 0) current_dpi_ = 96;

    HICON hIcon = LoadIcon(hInst_, MAKEINTRESOURCE(IDI_TRAY));
    SendMessage(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIcon));
    SendMessage(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIcon));

    ApplyShadow(hwnd);
    return 0;
}

LRESULT CalendarWindow::OnPaint(HWND hwnd, UINT, WPARAM, LPARAM) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);
    renderer_.Render(hdc, rc, model_, display_year_, display_month_, current_dpi_);
    EndPaint(hwnd, &ps);
    return 0;
}

LRESULT CalendarWindow::OnEraseBkgnd(HWND, UINT, WPARAM, LPARAM) {
    return 1;
}

LRESULT CalendarWindow::OnNcHitTest(HWND hwnd, UINT, WPARAM, LPARAM lp) {
    POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
    ScreenToClient(hwnd, &pt);
    RECT rc;
    GetClientRect(hwnd, &rc);

    int dpi = current_dpi_ > 0 ? current_dpi_ : 96;
    RECT title_rc = { rc.left, rc.top, rc.right,
                      rc.top + cfg::ScaleForDPI(cfg::kTitleBarHeight, dpi) };

    if (PtInRect(&title_rc, pt)) {
        int left_w = cfg::ScaleForDPI(cfg::kLeftPanelWidth, dpi);
        if (pt.x >= rc.left + left_w) {
            auto btn = renderer_.HitTestButton(pt.x, pt.y, rc, dpi);
            if (btn != CalendarRenderer::ButtonArea::None) return HTCLIENT;
        }
        return HTCAPTION;
    }
    return HTCLIENT;
}

LRESULT CalendarWindow::OnLButtonDown(HWND hwnd, UINT, WPARAM, LPARAM lp) {
    POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
    RECT rc;
    GetClientRect(hwnd, &rc);
    int dpi = current_dpi_ > 0 ? current_dpi_ : 96;

    auto btn = renderer_.HitTestButton(pt.x, pt.y, rc, dpi);
    switch (btn) {
    case CalendarRenderer::ButtonArea::PrevMonth: NavigateMonth(-1); return 0;
    case CalendarRenderer::ButtonArea::NextMonth: NavigateMonth(1); return 0;
    case CalendarRenderer::ButtonArea::Today: GoToday(); return 0;
    case CalendarRenderer::ButtonArea::Close: Hide(); return 0;
    default: break;
    }

    // 点击日历格子 → 选中
    int cell_idx = renderer_.HitTest(pt.x, pt.y, rc, dpi);
    if (cell_idx >= 0 && cell_idx < 42) {
        int row = cell_idx / 7;
        int col = cell_idx % 7;
        const CellData& cell = model_.GetCell(row, col);
        renderer_.SetSelectedCell(cell_idx, cell.solar_year, cell.solar_month, cell.solar_day);
        InvalidateRect(hwnd, nullptr, TRUE);
        UpdateWindow(hwnd);
    }
    return 0;
}

LRESULT CalendarWindow::OnActivate(HWND hwnd, UINT, WPARAM wp, LPARAM) {
    if (LOWORD(wp) == WA_INACTIVE && visible_) {
        // 立即视觉隐藏，延迟销毁给 ToggleShow 时间取消
        ShowWindow(hwnd, SW_HIDE);
        SetTimer(hwnd, kAutoHideTimerId, 200, nullptr);
    }
    return 0;
}

LRESULT CalendarWindow::OnTimer(HWND hwnd, UINT, WPARAM wp, LPARAM) {
    if (wp == kAutoHideTimerId) {
        KillTimer(hwnd, kAutoHideTimerId);
        if (visible_) Hide();
    }
    return 0;
}

LRESULT CalendarWindow::OnKeyDown(HWND hwnd, UINT, WPARAM wp, LPARAM) {
    switch (wp) {
    case VK_LEFT:  NavigateMonth(-1); return 0;
    case VK_RIGHT: NavigateMonth(1); return 0;
    case VK_ESCAPE: Hide(); return 0;
    case VK_HOME:  GoToday(); return 0;
    }
    return 0;
}

LRESULT CalendarWindow::OnDpiChanged(HWND hwnd, UINT, WPARAM wp, LPARAM lp) {
    auto* sr = reinterpret_cast<RECT*>(lp);
    current_dpi_ = HIWORD(wp);
    if (current_dpi_ <= 0) current_dpi_ = 96;
    SetWindowPos(hwnd, nullptr, sr->left, sr->top,
                 sr->right - sr->left, sr->bottom - sr->top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    InvalidateRect(hwnd, nullptr, TRUE);
    return 0;
}

LRESULT CalendarWindow::OnDestroy(HWND, UINT, WPARAM, LPARAM) {
    hwnd_ = nullptr;
    if (bg_brush_) { DeleteObject(bg_brush_); bg_brush_ = nullptr; }
    return 0;
}
