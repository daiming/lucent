#pragma once

#include <string>
#include <vector>

// 农历库适配器 - 封装 tyme4cpp 提供农历信息
class LunarProvider {
public:
    // 获取农历显示文本（优先显示节日名，否则显示农历日期）
    static std::wstring GetLunarText(int year, int month, int day);

    // 获取农历年月信息（如 "丙午年·马年"）
    static std::wstring GetLunarMonthYear(int year, int month);

    // 获取星期几文本（如 "星期四"）
    static std::wstring GetWeekdayText(int year, int month, int day);

    // 获取星期几（0=周日, 1=周一, ..., 6=周六）
    static int GetDayOfWeek(int year, int month, int day);

    // 判断是否为固定公历节日（如元旦、国庆等）
    static bool IsPublicHoliday(int year, int month, int day);

    // 获取今日农历日期文本（如 "五月十四"）
    static std::wstring GetLunarDayText(int year, int month, int day);

    // 获取宜（吉祥事项列表）
    static std::vector<std::wstring> GetYi(int year, int month, int day);

    // 获取忌（不宜事项列表）
    static std::vector<std::wstring> GetJi(int year, int month, int day);
};
