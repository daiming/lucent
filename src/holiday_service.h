#pragma once

#include <string>
#include <map>
#include <mutex>

// 日期类型枚举
enum class DayType {
    Normal,   // 普通工作日
    Holiday,  // 节假日
    Workday   // 调休补班日（周末上班）
};

// 假日数据条目
struct HolidayEntry {
    std::string name;  // 节假日名称（如 "元旦"），可为空
    std::string type;  // "public_holiday" 或 "workday"
};

// 假日数据服务：管理本地缓存和远程 API 获取
class HolidayService {
public:
    // 从本地文件加载假日数据
    void Load();

    // 保存假日数据到本地文件
    void Save();

    // 确保近3年数据存在（后台异步获取缺失年份）
    void EnsureRecentYears(int current_year);

    // 获取指定日期的类型
    DayType GetDayType(int year, int month, int day) const;

    // 获取指定日期的假日名称（如果有）
    std::wstring GetHolidayName(int year, int month, int day) const;

private:
    // 缓存：year -> (MMDD -> entry)
    std::map<int, std::map<std::string, HolidayEntry>> cache_;
    mutable std::mutex cache_mutex_;

    // 从 timor.tech API 获取指定年份数据
    bool FetchFromAPI(int year);

    // 检查缓存中是否有指定年份数据
    bool HasYearData(int year) const;

    // 获取数据文件路径
    std::wstring GetDataFilePath() const;
};
