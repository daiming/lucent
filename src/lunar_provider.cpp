/**
 * 农历库适配器
 * 封装 tyme4cpp 库提供农历日期、节日、宜忌等信息
 */

#include "lunar_provider.h"

#include <windows.h>
#include <sstream>
#include <string>

// tyme4cpp 头文件 - 必须放在最后（在所有 Windows 头文件之后）
#include "tyme.h"

using namespace tyme;

static std::wstring ToWString(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (len <= 0) return L"";
    std::wstring ws(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &ws[0], len);
    return ws;
}

std::wstring LunarProvider::GetLunarText(int year, int month, int day) {
    try {
        auto solar = SolarDay::from_ymd(year, month, day);
        auto lunar = solar.get_lunar_day();

        auto solar_festival = solar.get_festival();
        if (solar_festival.has_value()) {
            return ToWString(solar_festival.value().get_name());
        }

        auto lunar_festival = lunar.get_festival();
        if (lunar_festival.has_value()) {
            return ToWString(lunar_festival.value().get_name());
        }

        std::string lunar_day_name = lunar.get_name();
        if (lunar_day_name == u8"初一") {
            auto lunar_month = lunar.get_lunar_month();
            return ToWString(lunar_month.get_name());
        }

        return ToWString(lunar_day_name);
    } catch (...) {
        return L"";
    }
}

std::wstring LunarProvider::GetLunarMonthYear(int year, int month) {
    try {
        auto solar = SolarDay::from_ymd(year, month, 1);
        auto lunar = solar.get_lunar_day();

        int lunar_year_num = lunar.get_year();
        auto lunar_year = LunarYear::from_year(lunar_year_num);

        // 获取干支年名（去掉"农历"前缀）
        std::string full_name = lunar_year.get_name();
        // full_name 格式: "农历丙午年"
        std::string gz;
        size_t pos = full_name.find(u8"农历");
        if (pos != std::string::npos) {
            gz = full_name.substr(pos + 6); // 跳过 "农历" (6 bytes UTF-8)
        } else {
            gz = full_name;
        }

        std::string zodiac_name = lunar_year.get_sixty_cycle()
            .get_earth_branch().get_zodiac().get_name();

        std::string result = gz + u8"·" + zodiac_name + u8"年";
        return ToWString(result);
    } catch (...) {
        return L"";
    }
}

std::wstring LunarProvider::GetWeekdayText(int year, int month, int day) {
    static const wchar_t* names[] = {
        L"星期日", L"星期一", L"星期二", L"星期三",
        L"星期四", L"星期五", L"星期六"
    };
    int dow = GetDayOfWeek(year, month, day);
    if (dow >= 0 && dow <= 6) return names[dow];
    return L"";
}

int LunarProvider::GetDayOfWeek(int year, int month, int day) {
    try {
        auto solar = SolarDay::from_ymd(year, month, day);
        return solar.get_week().get_index();
    } catch (...) {
        SYSTEMTIME st = {};
        st.wYear = static_cast<WORD>(year);
        st.wMonth = static_cast<WORD>(month);
        st.wDay = static_cast<WORD>(day);
        SYSTEMTIME result;
        if (SystemTimeToTzSpecificLocalTime(nullptr, &st, &result)) {
            return result.wDayOfWeek;
        }
        return 0;
    }
}

bool LunarProvider::IsPublicHoliday(int year, int month, int day) {
    try {
        auto solar = SolarDay::from_ymd(year, month, day);
        if (solar.get_festival().has_value()) return true;
        auto lunar = solar.get_lunar_day();
        if (lunar.get_festival().has_value()) return true;
        return false;
    } catch (...) {
        return false;
    }
}

std::wstring LunarProvider::GetLunarDayText(int year, int month, int day) {
    try {
        auto solar = SolarDay::from_ymd(year, month, day);
        auto lunar = solar.get_lunar_day();
        auto lunar_month = lunar.get_lunar_month();
        std::string month_name = lunar_month.get_name();
        std::string day_name = lunar.get_name();
        return ToWString(month_name + day_name);
    } catch (...) {
        return L"";
    }
}

std::vector<std::wstring> LunarProvider::GetYi(int year, int month, int day) {
    try {
        auto solar = SolarDay::from_ymd(year, month, day);
        auto lunar = solar.get_lunar_day();
        auto recommends = lunar.get_recommends();
        std::vector<std::wstring> result;
        for (auto& r : recommends) {
            result.push_back(ToWString(r.get_name()));
        }
        return result;
    } catch (...) {
        return {};
    }
}

std::vector<std::wstring> LunarProvider::GetJi(int year, int month, int day) {
    try {
        auto solar = SolarDay::from_ymd(year, month, day);
        auto lunar = solar.get_lunar_day();
        auto avoids = lunar.get_avoids();
        std::vector<std::wstring> result;
        for (auto& a : avoids) {
            result.push_back(ToWString(a.get_name()));
        }
        return result;
    } catch (...) {
        return {};
    }
}
