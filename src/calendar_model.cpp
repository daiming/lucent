/**
 * 日历数据模型
 * 生成 6×7 网格的日历数据
 */

#include "calendar_model.h"
#include "lunar_provider.h"
#include <windows.h>

void CalendarModel::Populate(int year, int month, const HolidayService& holidays) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    int today_year = st.wYear;
    int today_month = st.wMonth;
    int today_day = st.wDay;

    int first_dow = LunarProvider::GetDayOfWeek(year, month, 1);
    int days_in_month = GetDaysInMonth(year, month);

    int prev_month = month - 1;
    int prev_year = year;
    if (prev_month < 1) { prev_month = 12; prev_year--; }
    int days_in_prev = GetDaysInMonth(prev_year, prev_month);

    int next_month = month + 1;
    int next_year = year;
    if (next_month > 12) { next_month = 1; next_year++; }

    int day_counter = 0;
    int next_day = 1;  // 下月日期计数器（修复：独立计数）

    for (int i = 0; i < 42; i++) {
        CellData& cell = cells_[i];

        if (i < first_dow) {
            int d = days_in_prev - first_dow + i + 1;
            cell.solar_year = prev_year;
            cell.solar_month = prev_month;
            cell.solar_day = d;
            cell.is_in_month = false;
        } else if (day_counter < days_in_month) {
            day_counter++;
            cell.solar_year = year;
            cell.solar_month = month;
            cell.solar_day = day_counter;
            cell.is_in_month = true;
        } else {
            // 修复：使用独立的 next_day 计数器
            cell.solar_year = next_year;
            cell.solar_month = next_month;
            cell.solar_day = next_day++;
            cell.is_in_month = false;
        }

        // 农历文本
        cell.lunar_text = LunarProvider::GetLunarText(
            cell.solar_year, cell.solar_month, cell.solar_day);

        // 日期类型
        cell.day_type = holidays.GetDayType(
            cell.solar_year, cell.solar_month, cell.solar_day);

        // 节假日名称（仅覆盖节日名到 lunar_text）
        cell.holiday_name = holidays.GetHolidayName(
            cell.solar_year, cell.solar_month, cell.solar_day);

        // 有名称的法定假日 → 只在假期第一天显示节日名，其余天显示农历
        // 补班日（workday）不替换农历文本，保留原始农历显示
        if (!cell.holiday_name.empty() && cell.day_type == DayType::Holiday) {
            bool prev_same = (i > 0 &&
                cells_[i - 1].holiday_name == cell.holiday_name);
            if (!prev_same) {
                cell.lunar_text = cell.holiday_name;
            }
        }

        cell.is_today = (cell.solar_year == today_year &&
                         cell.solar_month == today_month &&
                         cell.solar_day == today_day);
    }
}

const CellData& CalendarModel::GetCell(int row, int col) const {
    return cells_[row * 7 + col];
}

int CalendarModel::GetDaysInMonth(int year, int month) {
    static const int days[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (month == 2) {
        if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
            return 29;
        }
    }
    return days[month];
}
