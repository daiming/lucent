#pragma once

#include <array>
#include <string>
#include "holiday_service.h"

// 日历单元格数据
struct CellData {
    int solar_day = 0;        // 公历日期数字 (1-31)
    int solar_month = 0;      // 月份 (1-12)
    int solar_year = 0;       // 年份
    std::wstring lunar_text;  // 农历/节日文本
    std::wstring holiday_name; // 节假日名称（仅真正的节日当天有值）
    DayType day_type = DayType::Normal;
    bool is_today = false;    // 是否为今天
    bool is_in_month = false; // 是否属于当前显示月份
};

// 日历月份数据模型
// 生成 6×7 (42格) 的日历网格数据
class CalendarModel {
public:
    CalendarModel() = default;

    // 填充指定年月的数据
    void Populate(int year, int month, const HolidayService& holidays);

    // 获取指定位置的单元格数据
    const CellData& GetCell(int row, int col) const;

    // 获取总单元格数
    int GetCellCount() const { return 42; }

    // 清空数据，释放字符串内存
    void Clear() {
        for (auto& c : cells_) {
            c.lunar_text.clear();
            c.lunar_text.shrink_to_fit();
            c.holiday_name.clear();
            c.holiday_name.shrink_to_fit();
        }
    }

private:
    std::array<CellData, 42> cells_{};

    // 获取指定年月的天数
    static int GetDaysInMonth(int year, int month);
};
