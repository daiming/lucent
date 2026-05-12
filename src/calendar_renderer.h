#pragma once

#include <windows.h>
#include "calendar_model.h"

namespace Gdiplus { struct Graphics; struct GraphicsPath; }

// GDI 日历绘制引擎 - 左右分栏布局
class CalendarRenderer {
public:
    CalendarRenderer() = default;
    ~CalendarRenderer();

    static void InitGdiPlus();
    static void ShutdownGdiPlus();
    void CleanupFonts();

    void Render(HDC hdc, const RECT& rc, const CalendarModel& model,
                int display_year, int display_month, int dpi);

    // 选中单元格
    void SetSelectedCell(int cell_index, int year, int month, int day);
    int GetSelectedCell() const { return selected_cell_; }
    int GetSelectedYear() const { return selected_year_; }
    int GetSelectedMonth() const { return selected_month_; }
    int GetSelectedDay() const { return selected_day_; }

    // 命中测试
    int HitTest(int x, int y, const RECT& rc, int dpi) const;

    enum class ButtonArea { None, PrevMonth, NextMonth, Today, Close };
    ButtonArea HitTestButton(int x, int y, const RECT& rc, int dpi) const;

private:
    HFONT title_font_ = nullptr;
    HFONT day_font_ = nullptr;
    HFONT lunar_font_ = nullptr;
    HFONT weekday_font_ = nullptr;
    HFONT big_date_font_ = nullptr;   // 左侧大日期
    HFONT label_font_ = nullptr;      // 标签字体
    HFONT yiji_font_ = nullptr;       // 宜忌内容字体
    HFONT badge_font_ = nullptr;      // 徽章小字体
    int current_dpi_ = 96;

    // 选中状态
    int selected_cell_ = -1;          // 选中的单元格索引 (-1=无)
    int selected_year_ = 0;
    int selected_month_ = 0;
    int selected_day_ = 0;

    void UpdateFonts(int dpi);

    // 各区域绘制
    void DrawLeftPanel(HDC hdc, const RECT& rc, int year, int month, int day, int dpi);
    void DrawRightPanel(HDC hdc, const RECT& rc, const CalendarModel& model,
                         int year, int month, int dpi);
    void DrawTitle(HDC hdc, const RECT& rc, int year, int month, int dpi);
    void DrawWeekdayHeader(HDC hdc, const RECT& rc, int dpi);
    void DrawGrid(HDC hdc, const RECT& rc, const CalendarModel& model, int dpi);
    void DrawCell(HDC hdc, struct Gdiplus::Graphics& graphics, struct Gdiplus::GraphicsPath& path,
                  const RECT& cell_rc, const CellData& cell, int cell_w, int cell_h, bool is_selected);

    // 区域计算
    RECT GetLeftPanelRect(const RECT& rc, int dpi) const;
    RECT GetRightPanelRect(const RECT& rc, int dpi) const;
    RECT GetTitleRect(const RECT& rc, int dpi) const;
    RECT GetWeekdayRect(const RECT& rc, int dpi) const;
    RECT GetGridRect(const RECT& rc, int dpi) const;
    RECT GetCellRect(const RECT& grid_rc, int row, int col, int cell_w, int cell_h) const;

    // 按钮矩形
    RECT GetPrevButtonRect(const RECT& title_rc, int dpi) const;
    RECT GetNextButtonRect(const RECT& title_rc, int dpi) const;
    RECT GetTodayButtonRect(const RECT& title_rc, int dpi) const;
    RECT GetCloseButtonRect(const RECT& rc, int dpi) const;
};
