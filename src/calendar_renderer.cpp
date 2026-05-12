/**
 * GDI 日历绘制引擎 - 左右分栏布局
 *
 * +-------------------+-------------------------------------------+
 * |                   |  <  2026年5月  >            [今天] [×]     |
 * |   星期四          +-------------------------------------------+
 * |                   |  日   一   二   三   四   五   六          |
 * |     8             |                                           |
 * |                   |  ... 6x7 日期网格（无网格线）...            |
 * |  五月十四         |                                           |
 * | 丙午年·马年       |                                           |
 * |                   |                                           |
 * |   宜              |                                           |
 * |  嫁娶 祭祀        |                                           |
 * |                   |                                           |
 * |   忌              |                                           |
 * |  开市 动土        |                                           |
 * +-------------------+-------------------------------------------+
 */

#include "calendar_renderer.h"
#include "config.h"
#include "lunar_provider.h"
#include <windows.h>
#include <algorithm>
#include <objidl.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "msimg32.lib")

// GDI+ 全局初始化（必须在使用任何 GDI+ 功能前调用）
static ULONG_PTR g_gdiplus_token = 0;

void CalendarRenderer::InitGdiPlus() {
    if (g_gdiplus_token == 0) {
        Gdiplus::GdiplusStartupInput input;
        Gdiplus::GdiplusStartup(&g_gdiplus_token, &input, nullptr);
    }
}

void CalendarRenderer::ShutdownGdiPlus() {
    if (g_gdiplus_token) {
        Gdiplus::GdiplusShutdown(g_gdiplus_token);
        g_gdiplus_token = 0;
    }
}

CalendarRenderer::~CalendarRenderer() { CleanupFonts(); }

// 构建圆角路径（复用）
static void BuildRoundPath(Gdiplus::GraphicsPath& path, const RECT& rc, int radius) {
    path.Reset();
    int r = radius;
    int x = rc.left, y = rc.top, w = rc.right - rc.left, h = rc.bottom - rc.top;
    path.AddArc(x, y, r * 2, r * 2, 180, 90);
    path.AddArc(x + w - r * 2, y, r * 2, r * 2, 270, 90);
    path.AddArc(x + w - r * 2, y + h - r * 2, r * 2, r * 2, 0, 90);
    path.AddArc(x, y + h - r * 2, r * 2, r * 2, 90, 90);
    path.CloseFigure();
}

// GDI+ 抗锯齿圆角矩形填充（复用外部 Graphics 对象）
static void FillRoundRect(Gdiplus::Graphics& graphics, Gdiplus::GraphicsPath& path,
                           const RECT& rc, int radius, COLORREF color) {
    BuildRoundPath(path, rc, radius);
    Gdiplus::Color c(255, GetRValue(color), GetGValue(color), GetBValue(color));
    Gdiplus::SolidBrush brush(c);
    graphics.FillPath(&brush, &path);
}

// GDI+ 抗锯齿圆角矩形边框（复用外部 Graphics 对象）
static void DrawRoundRect(Gdiplus::Graphics& graphics, Gdiplus::GraphicsPath& path,
                           const RECT& rc, int radius, COLORREF color, int width) {
    BuildRoundPath(path, rc, radius);
    Gdiplus::Color c(255, GetRValue(color), GetGValue(color), GetBValue(color));
    Gdiplus::Pen pen(c, (Gdiplus::REAL)width);
    graphics.DrawPath(&pen, &path);
}

void CalendarRenderer::SetSelectedCell(int cell_index, int year, int month, int day) {
    selected_cell_ = cell_index;
    selected_year_ = year;
    selected_month_ = month;
    selected_day_ = day;
}

void CalendarRenderer::CleanupFonts() {
    auto del = [](HFONT& f) { if (f) { DeleteObject(f); f = nullptr; } };
    del(title_font_); del(day_font_); del(lunar_font_);
    del(weekday_font_); del(big_date_font_); del(label_font_);
    del(yiji_font_); del(badge_font_);
}

void CalendarRenderer::UpdateFonts(int dpi) {
    if (dpi == current_dpi_ && title_font_) return;
    CleanupFonts();
    current_dpi_ = dpi;

    auto mk = [&](int pt, int weight) -> HFONT {
        return CreateFontW(cfg::FontHeightForDPI(pt, dpi), 0, 0, 0, weight,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    };
    title_font_ = mk(15, FW_BOLD);
    day_font_ = mk(14, FW_NORMAL);
    lunar_font_ = mk(9, FW_NORMAL);
    weekday_font_ = mk(10, FW_NORMAL);
    big_date_font_ = mk(42, FW_BOLD);
    label_font_ = mk(11, FW_BOLD);
    yiji_font_ = mk(10, FW_NORMAL);
    badge_font_ = mk(7, FW_BOLD);
}

// --- 区域计算 ---

RECT CalendarRenderer::GetLeftPanelRect(const RECT& rc, int dpi) const {
    int w = cfg::ScaleForDPI(cfg::kLeftPanelWidth, dpi);
    return { rc.left, rc.top, rc.left + w, rc.bottom };
}

RECT CalendarRenderer::GetRightPanelRect(const RECT& rc, int dpi) const {
    int w = cfg::ScaleForDPI(cfg::kLeftPanelWidth, dpi);
    return { rc.left + w, rc.top, rc.right, rc.bottom };
}

RECT CalendarRenderer::GetTitleRect(const RECT& rc, int dpi) const {
    int h = cfg::ScaleForDPI(cfg::kTitleBarHeight, dpi);
    return { rc.left, rc.top, rc.right, rc.top + h };
}

RECT CalendarRenderer::GetWeekdayRect(const RECT& rc, int dpi) const {
    int top = cfg::ScaleForDPI(cfg::kTitleBarHeight, dpi);
    int h = cfg::ScaleForDPI(cfg::kWeekdayHeaderHeight, dpi);
    int pad = cfg::ScaleForDPI(10, dpi);
    // 与 GetGridRect 使用相同的左右 padding，确保列对齐
    return { rc.left + pad, rc.top + top, rc.right - pad, rc.top + top + h };
}

RECT CalendarRenderer::GetGridRect(const RECT& rc, int dpi) const {
    int top = cfg::ScaleForDPI(cfg::kTitleBarHeight + cfg::kWeekdayHeaderHeight, dpi);
    int pad = cfg::ScaleForDPI(10, dpi);
    return { rc.left + pad, rc.top + top + pad, rc.right - pad, rc.bottom - pad };
}

RECT CalendarRenderer::GetCellRect(const RECT& grid_rc, int row, int col,
                                     int cell_w, int cell_h) const {
    int x = grid_rc.left + col * cell_w;
    int y = grid_rc.top + row * cell_h;
    return { x, y, x + cell_w, y + cell_h };
}

RECT CalendarRenderer::GetPrevButtonRect(const RECT& title_rc, int dpi) const {
    int sz = cfg::ScaleForDPI(22, dpi);
    int m = cfg::ScaleForDPI(6, dpi);
    int cy = (title_rc.top + title_rc.bottom) / 2;
    return { title_rc.left + m, cy - sz / 2, title_rc.left + m + sz, cy + sz / 2 };
}

RECT CalendarRenderer::GetNextButtonRect(const RECT& title_rc, int dpi) const {
    int sz = cfg::ScaleForDPI(22, dpi);
    int m = cfg::ScaleForDPI(6, dpi);
    int off = cfg::ScaleForDPI(28, dpi);
    int cy = (title_rc.top + title_rc.bottom) / 2;
    int left = title_rc.left + m + off;
    return { left, cy - sz / 2, left + sz, cy + sz / 2 };
}

RECT CalendarRenderer::GetTodayButtonRect(const RECT& title_rc, int dpi) const {
    int m = cfg::ScaleForDPI(8, dpi);
    int bw = cfg::ScaleForDPI(44, dpi);
    int bh = cfg::ScaleForDPI(22, dpi);
    int cy = (title_rc.top + title_rc.bottom) / 2;
    return { title_rc.right - m - bw, cy - bh / 2, title_rc.right - m, cy + bh / 2 };
}

RECT CalendarRenderer::GetCloseButtonRect(const RECT& rc, int dpi) const {
    int sz = cfg::ScaleForDPI(22, dpi);
    int m = cfg::ScaleForDPI(6, dpi);
    int cy = (rc.top + rc.bottom) / 2;
    return { rc.right - m - sz, cy - sz / 2, rc.right - m, cy + sz / 2 };
}

// --- 渲染主函数 ---

void CalendarRenderer::Render(HDC hdc, const RECT& rc, const CalendarModel& model,
                               int display_year, int display_month, int dpi) {
    UpdateFonts(dpi);

    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;

    // 双缓冲 - 使用 32 位 DIB 确保 alpha 通道可控
    HDC mem_dc = CreateCompatibleDC(hdc);
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;  // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP mem_bmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HBITMAP old_bmp = static_cast<HBITMAP>(SelectObject(mem_dc, mem_bmp));

    // 黑色背景（DWM margins=-1 时，黑色像素 alpha=0 被视为透明）
    HBRUSH bg = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(mem_dc, &rc, bg);
    DeleteObject(bg);

    // 右面板区域填充白色
    RECT right_panel = GetRightPanelRect(rc, dpi);
    HBRUSH white_bg = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(mem_dc, &right_panel, white_bg);
    DeleteObject(white_bg);

    // 绘制左右面板
    int info_year = selected_year_ > 0 ? selected_year_ : display_year;
    int info_month = selected_month_ > 0 ? selected_month_ : display_month;
    int info_day = selected_day_ > 0 ? selected_day_ : 0;
    DrawLeftPanel(mem_dc, rc, info_year, info_month, info_day, dpi);
    DrawRightPanel(mem_dc, rc, model, display_year, display_month, dpi);

    // 修正 alpha 通道：非黑色像素设为不透明(alpha=255)，黑色像素保持透明(alpha=0)
    DWORD* pixels = static_cast<DWORD*>(bits);
    int total = width * height;
    for (int i = 0; i < total; i++) {
        if (pixels[i] & 0x00FFFFFF) {   // RGB 不全为 0
            pixels[i] |= 0xFF000000;     // alpha = 255
        } else {
            pixels[i] = 0x00000000;      // 纯透明
        }
    }

    AlphaBlend(hdc, rc.left, rc.top, width, height, mem_dc, 0, 0, width, height,
               { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA });
    SelectObject(mem_dc, old_bmp);
    DeleteObject(mem_bmp);
    DeleteDC(mem_dc);
}

// --- 左侧面板 ---

void CalendarRenderer::DrawLeftPanel(HDC hdc, const RECT& rc, int year, int month, int day, int dpi) {
    RECT left = GetLeftPanelRect(rc, dpi);
    int m = cfg::ScaleForDPI(10, dpi);

    // 左侧面板背景
    HBRUSH lbg = CreateSolidBrush(cfg::kColorLeftBg);
    FillRect(hdc, &left, lbg);
    DeleteObject(lbg);

    // 分割线
    HPEN pen = CreatePen(PS_SOLID, 1, cfg::kColorDivider);
    HPEN old_pen = static_cast<HPEN>(SelectObject(hdc, pen));
    MoveToEx(hdc, left.right - 1, left.top, nullptr);
    LineTo(hdc, left.right - 1, left.bottom);
    SelectObject(hdc, old_pen);
    DeleteObject(pen);

    // 如果没有选中日期，使用今天
    if (day <= 0) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        year = st.wYear;
        month = st.wMonth;
        day = st.wDay;
    }

    // 星期几
    HFONT old_font = static_cast<HFONT>(SelectObject(hdc, weekday_font_));
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, cfg::kColorLunar);
    int top_pad = m + cfg::ScaleForDPI(6, dpi);
    RECT r = { left.left + m, left.top + top_pad, left.right - m, left.top + top_pad + cfg::ScaleForDPI(20, dpi) };
    std::wstring week_text = LunarProvider::GetWeekdayText(year, month, day);
    DrawTextW(hdc, week_text.c_str(), -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // 大日期数字
    SelectObject(hdc, big_date_font_);
    SetTextColor(hdc, cfg::kColorToday);
    wchar_t day_str[8];
    swprintf_s(day_str, L"%d", day);
    int big_h = cfg::ScaleForDPI(60, dpi);
    r = { left.left + m, r.bottom, left.right - m, r.bottom + big_h };
    DrawTextW(hdc, day_str, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // 农历日期
    SelectObject(hdc, weekday_font_);
    SetTextColor(hdc, cfg::kColorNormal);
    std::wstring lunar_day = LunarProvider::GetLunarDayText(year, month, day);
    r.top = r.bottom;
    r.bottom = r.top + cfg::ScaleForDPI(24, dpi);
    DrawTextW(hdc, lunar_day.c_str(), -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // 农历年信息
    SelectObject(hdc, lunar_font_);
    SetTextColor(hdc, cfg::kColorLunar);
    std::wstring lunar_year = LunarProvider::GetLunarMonthYear(year, month);
    r.top = r.bottom;
    r.bottom = r.top + cfg::ScaleForDPI(20, dpi);
    DrawTextW(hdc, lunar_year.c_str(), -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // 分隔线
    pen = CreatePen(PS_SOLID, 1, cfg::kColorDivider);
    old_pen = static_cast<HPEN>(SelectObject(hdc, pen));
    int sep_y = r.bottom + cfg::ScaleForDPI(8 + 6, dpi);
    MoveToEx(hdc, left.left + m, sep_y, nullptr);
    LineTo(hdc, left.right - m, sep_y);
    SelectObject(hdc, old_pen);
    DeleteObject(pen);

    // 宜
    r.top = sep_y + cfg::ScaleForDPI(6 + 6, dpi);
    r.bottom = r.top + cfg::ScaleForDPI(18, dpi);
    SelectObject(hdc, label_font_);
    SetTextColor(hdc, cfg::kColorRestBadge);
    DrawTextW(hdc, L"宜", 1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    auto yi = LunarProvider::GetYi(year, month, day);
    SelectObject(hdc, yiji_font_);
    SetTextColor(hdc, cfg::kColorNormal);
    r.top = r.bottom + cfg::ScaleForDPI(10, dpi);
    r.bottom = r.top + cfg::ScaleForDPI(60, dpi);
    std::wstring yi_text;
    for (size_t i = 0; i < yi.size() && i < 5; i++) {
        if (i > 0) yi_text += L" ";
        yi_text += yi[i];
    }
    if (yi_text.empty()) yi_text = L"无";
    DrawTextW(hdc, yi_text.c_str(), -1, &r, DT_LEFT | DT_TOP | DT_WORDBREAK);

    // 忌
    r.top = r.bottom + cfg::ScaleForDPI(4, dpi);
    r.bottom = r.top + cfg::ScaleForDPI(18, dpi);
    SelectObject(hdc, label_font_);
    SetTextColor(hdc, cfg::kColorHoliday);
    DrawTextW(hdc, L"忌", 1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    auto ji = LunarProvider::GetJi(year, month, day);
    SelectObject(hdc, yiji_font_);
    SetTextColor(hdc, cfg::kColorNormal);
    r.top = r.bottom + cfg::ScaleForDPI(10, dpi);
    r.bottom = r.top + cfg::ScaleForDPI(60, dpi);
    std::wstring ji_text;
    for (size_t i = 0; i < ji.size() && i < 5; i++) {
        if (i > 0) ji_text += L" ";
        ji_text += ji[i];
    }
    if (ji_text.empty()) ji_text = L"无";
    DrawTextW(hdc, ji_text.c_str(), -1, &r, DT_LEFT | DT_TOP | DT_WORDBREAK);

    SelectObject(hdc, old_font);
}

// --- 右侧面板 ---

void CalendarRenderer::DrawRightPanel(HDC hdc, const RECT& rc,
                                        const CalendarModel& model,
                                        int year, int month, int dpi) {
    RECT right = GetRightPanelRect(rc, dpi);
    DrawTitle(hdc, right, year, month, dpi);
    DrawWeekdayHeader(hdc, right, dpi);
    DrawGrid(hdc, right, model, dpi);
}

void CalendarRenderer::DrawTitle(HDC hdc, const RECT& rc, int year, int month, int dpi) {
    RECT title_rc = GetTitleRect(rc, dpi);

    // 底部分割线
    HPEN pen = CreatePen(PS_SOLID, 1, cfg::kColorDivider);
    HPEN old_pen = static_cast<HPEN>(SelectObject(hdc, pen));
    MoveToEx(hdc, title_rc.left, title_rc.bottom - 1, nullptr);
    LineTo(hdc, title_rc.right, title_rc.bottom - 1);
    SelectObject(hdc, old_pen);
    DeleteObject(pen);

    SetBkMode(hdc, TRANSPARENT);
    HFONT old_font = static_cast<HFONT>(SelectObject(hdc, weekday_font_));

    // < 按钮
    RECT prev_rc = GetPrevButtonRect(title_rc, dpi);
    SetTextColor(hdc, RGB(80, 80, 80));
    DrawTextW(hdc, L"<", 1, &prev_rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // > 按钮
    RECT next_rc = GetNextButtonRect(title_rc, dpi);
    DrawTextW(hdc, L">", 1, &next_rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // 年月
    wchar_t txt[32];
    swprintf_s(txt, L"%d年%d月", year, month);
    SelectObject(hdc, title_font_);
    SetTextColor(hdc, RGB(40, 40, 40));
    RECT text_rc = title_rc;
    int off = cfg::ScaleForDPI(60, dpi);
    text_rc.left = title_rc.left + off;
    text_rc.right = GetTodayButtonRect(title_rc, dpi).left;
    DrawTextW(hdc, txt, -1, &text_rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // 今天按钮
    RECT today_rc = GetTodayButtonRect(title_rc, dpi);
    SetTextColor(hdc, cfg::kColorToday);
    SelectObject(hdc, weekday_font_);
    DrawTextW(hdc, L"今天", 2, &today_rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, old_font);
}

void CalendarRenderer::DrawWeekdayHeader(HDC hdc, const RECT& rc, int dpi) {
    RECT week_rc = GetWeekdayRect(rc, dpi);
    HFONT old_font = static_cast<HFONT>(SelectObject(hdc, weekday_font_));
    SetBkMode(hdc, TRANSPARENT);

    const wchar_t* days[] = { L"日", L"一", L"二", L"三", L"四", L"五", L"六" };
    int cw = (week_rc.right - week_rc.left) / 7;
    for (int i = 0; i < 7; i++) {
        RECT c = { week_rc.left + i * cw, week_rc.top,
                    week_rc.left + (i + 1) * cw, week_rc.bottom };
        SetTextColor(hdc, (i == 0 || i == 6) ? cfg::kColorHoliday : RGB(100, 100, 100));
        DrawTextW(hdc, days[i], -1, &c, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // 底部分割线
    HPEN pen = CreatePen(PS_SOLID, 1, cfg::kColorDivider);
    HPEN old_pen = static_cast<HPEN>(SelectObject(hdc, pen));
    MoveToEx(hdc, week_rc.left, week_rc.bottom - 1, nullptr);
    LineTo(hdc, week_rc.right, week_rc.bottom - 1);
    SelectObject(hdc, old_pen);
    DeleteObject(pen);

    SelectObject(hdc, old_font);
}

void CalendarRenderer::DrawGrid(HDC hdc, const RECT& rc,
                                  const CalendarModel& model, int dpi) {
    RECT grid_rc = GetGridRect(rc, dpi);
    int cw = (grid_rc.right - grid_rc.left) / 7;
    int ch = cfg::ScaleForDPI(cfg::kCellHeight, dpi);

    // 共享 GDI+ 对象，避免每个格子重复创建
    Gdiplus::Graphics graphics(hdc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
    Gdiplus::GraphicsPath path;

    for (int row = 0; row < 6; row++) {
        for (int col = 0; col < 7; col++) {
            RECT cell_rc = GetCellRect(grid_rc, row, col, cw, ch);
            const CellData& cell = model.GetCell(row, col);
            bool is_selected = (cell.solar_year == selected_year_ &&
                                cell.solar_month == selected_month_ &&
                                cell.solar_day == selected_day_);
            DrawCell(hdc, graphics, path, cell_rc, cell, cw, ch, is_selected);
        }
    }
}

void CalendarRenderer::DrawCell(HDC hdc, Gdiplus::Graphics& graphics,
                                  Gdiplus::GraphicsPath& path,
                                  const RECT& cell_rc, const CellData& cell,
                                  int cell_w, int cell_h, bool is_selected) {
    int dpi = current_dpi_;
    int pad = cfg::ScaleForDPI(4, dpi);
    int radius = cfg::ScaleForDPI(10, dpi);

    SetBkMode(hdc, TRANSPARENT);

    bool is_holiday = (cell.day_type == DayType::Holiday);
    bool is_workday = (cell.day_type == DayType::Workday);
    bool has_holiday_name = !cell.holiday_name.empty();

    RECT bg = { cell_rc.left + pad, cell_rc.top + pad,
                 cell_rc.right - pad, cell_rc.top + pad + cell_h - pad * 2 };

    // ---- 背景：today > workday > holiday > normal ----
    if (cell.is_today) {
        FillRoundRect(graphics, path, bg, radius, cfg::kColorTodayBg);
    } else if (is_workday) {
        FillRoundRect(graphics, path, bg, radius, cfg::kColorWorkBg);
    } else if (is_holiday) {
        FillRoundRect(graphics, path, bg, radius, cfg::kColorHolidayBg);
    }

    // ---- 选中：蓝色边框（在角标下层） ----
    if (is_selected) {
        DrawRoundRect(graphics, path, bg, radius, cfg::kColorSelectedBorder, 2);
    }

    // ---- 角标：始终在最上层 ----
    int badge_sz = cfg::ScaleForDPI(18, dpi);
    int half = badge_sz / 2;
    RECT badge_rc = { bg.right - half, bg.top - half, bg.right + half, bg.top + half };

    if (is_holiday) {
        FillRoundRect(graphics, path, badge_rc, cfg::ScaleForDPI(8, dpi), cfg::kColorRestBadge);
        HFONT old_f = static_cast<HFONT>(SelectObject(hdc, badge_font_));
        SetTextColor(hdc, RGB(255, 255, 255));
        DrawTextW(hdc, L"休", 1, &badge_rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, old_f);
    } else if (is_workday) {
        FillRoundRect(graphics, path, badge_rc, cfg::ScaleForDPI(8, dpi), cfg::kColorWorkBadge);
        HFONT old_f = static_cast<HFONT>(SelectObject(hdc, badge_font_));
        SetTextColor(hdc, RGB(255, 255, 255));
        DrawTextW(hdc, L"班", 1, &badge_rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, old_f);
    }

    // ---- 公历日期数字 ----
    HFONT old_font = static_cast<HFONT>(SelectObject(hdc, day_font_));
    wchar_t day_text[4];
    swprintf_s(day_text, L"%d", cell.solar_day);

    RECT day_rc = { cell_rc.left, cell_rc.top,
                    cell_rc.right, cell_rc.top + cfg::ScaleForDPI(34, dpi) };

    if (cell.is_today) {
        SetTextColor(hdc, cfg::kColorToday);
    } else if (!cell.is_in_month) {
        SetTextColor(hdc, cfg::kColorOtherMonth);
    } else if (is_workday) {
        SetTextColor(hdc, cfg::kColorWorkday);
    } else if (has_holiday_name) {
        SetTextColor(hdc, cfg::kColorHolidayText);
    } else if (is_holiday) {
        SetTextColor(hdc, cfg::kColorHolidayText);
    } else {
        SetTextColor(hdc, cfg::kColorNormal);
    }

    DrawTextW(hdc, day_text, -1, &day_rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // ---- 农历/节日文本（所有日期都显示，包括非当月） ----
    if (!cell.lunar_text.empty()) {
        SelectObject(hdc, lunar_font_);

        RECT lunar_rc = { cell_rc.left, cell_rc.top + cfg::ScaleForDPI(32, dpi),
                          cell_rc.right, cell_rc.bottom - cfg::ScaleForDPI(2, dpi) };

        if (!cell.is_in_month) {
            SetTextColor(hdc, cfg::kColorOtherMonth);
        } else if (cell.is_today) {
            SetTextColor(hdc, cfg::kColorToday);
        } else if (is_workday) {
            SetTextColor(hdc, cfg::kColorWorkday);
        } else if (has_holiday_name) {
            SetTextColor(hdc, cfg::kColorHolidayText);
        } else if (is_holiday) {
            SetTextColor(hdc, cfg::kColorHolidayText);
        } else {
            SetTextColor(hdc, cfg::kColorLunar);
        }

        std::wstring disp = cell.lunar_text;
        if (disp.length() > 4) disp = disp.substr(0, 4);
        DrawTextW(hdc, disp.c_str(), static_cast<int>(disp.length()),
                  &lunar_rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    SelectObject(hdc, old_font);
}

// --- 命中测试 ---

int CalendarRenderer::HitTest(int x, int y, const RECT& rc, int dpi) const {
    RECT grid_rc = GetGridRect(GetRightPanelRect(rc, dpi), dpi);
    if (x < grid_rc.left || x >= grid_rc.right || y < grid_rc.top || y >= grid_rc.bottom)
        return -1;

    int cw = (grid_rc.right - grid_rc.left) / 7;
    int ch = cfg::ScaleForDPI(cfg::kCellHeight, dpi);
    int col = (x - grid_rc.left) / cw;
    int row = (y - grid_rc.top) / ch;
    if (col < 0 || col >= 7 || row < 0 || row >= 6) return -1;
    return row * 7 + col;
}

CalendarRenderer::ButtonArea CalendarRenderer::HitTestButton(int x, int y,
                                                              const RECT& rc, int dpi) const {
    RECT right = GetRightPanelRect(rc, dpi);
    RECT title_rc = GetTitleRect(right, dpi);

    auto hit = [](const RECT& r, int px, int py) -> bool {
        return px >= r.left && px <= r.right && py >= r.top && py <= r.bottom;
    };

    if (hit(GetPrevButtonRect(title_rc, dpi), x, y)) return ButtonArea::PrevMonth;
    if (hit(GetNextButtonRect(title_rc, dpi), x, y)) return ButtonArea::NextMonth;
    if (hit(GetTodayButtonRect(title_rc, dpi), x, y)) return ButtonArea::Today;
    return ButtonArea::None;
}
