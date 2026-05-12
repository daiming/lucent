/**
 * 假日数据服务
 * 管理本地 JSON 缓存和远程 API 数据获取
 * 支持后台异步加载近3年数据，不阻塞 UI
 */

#include "holiday_service.h"
#include "config.h"
#include "http_client.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <thread>
#include <ctime>
#include <algorithm>

using json = nlohmann::json;

std::wstring HolidayService::GetDataFilePath() const {
    return cfg::GetAppDataPath() + cfg::kHolidaysFile;
}

bool HolidayService::HasYearData(int year) const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto it = cache_.find(year);
    return it != cache_.end() && !it->second.empty();
}

void HolidayService::Load() {
    std::wstring wpath = GetDataFilePath();

    std::ifstream ifs(wpath);
    if (!ifs.is_open()) return;

    try {
        json j = json::parse(ifs);
        if (!j.contains("data") || !j["data"].is_object()) return;

        std::lock_guard<std::mutex> lock(cache_mutex_);
        for (auto& [year_key, year_data] : j["data"].items()) {
            int year = std::stoi(year_key);
            if (!year_data.is_object()) continue;

            for (auto& [mmdd_key, entry] : year_data.items()) {
                if (!entry.is_object()) continue;
                HolidayEntry he;
                he.name = entry.value("name", "");
                he.type = entry.value("type", "");
                cache_[year][mmdd_key] = he;
            }
        }
    } catch (const json::exception&) {
        // 解析失败，使用空缓存
    }
}

void HolidayService::Save() {
    std::wstring wpath = GetDataFilePath();

    json j;
    j["version"] = 2;

    auto now = std::time(nullptr);
    struct tm tm_buf;
    gmtime_s(&tm_buf, &now);
    char timebuf[64];
    std::strftime(timebuf, sizeof(timebuf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
    j["last_updated"] = timebuf;

    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        for (auto& [year, year_map] : cache_) {
            std::string year_key = std::to_string(year);
            for (auto& [mmdd, entry] : year_map) {
                j["data"][year_key][mmdd] = {
                    {"name", entry.name},
                    {"type", entry.type}
                };
            }
        }
    }

    std::ofstream ofs(wpath);
    if (ofs.is_open()) {
        ofs << j.dump(2);
    }
}

bool HolidayService::FetchFromAPI(int year) {
    std::wstring path = std::wstring(cfg::kApiPathPrefix) + std::to_wstring(year) + L"/";

    std::string response = HttpClient::HttpGet(cfg::kApiHost, path);
    if (response.empty()) return false;

    try {
        json j = json::parse(response);
        response.clear();
        response.shrink_to_fit();

        if (j.value("code", -1) != 0) return false;
        if (!j.contains("holiday") || !j["holiday"].is_object()) return false;

        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            for (auto& [date_key, entry] : j["holiday"].items()) {
                std::string mmdd = date_key;
                mmdd.erase(std::remove(mmdd.begin(), mmdd.end(), '-'), mmdd.end());

                HolidayEntry he;
                he.name = entry.value("name", "");
                bool is_holiday = entry.value("holiday", false);
                he.type = is_holiday ? "public_holiday" : "workday";

                cache_[year][mmdd] = he;
            }
        }

        Save();
        return true;
    } catch (const json::exception&) {
        return false;
    }
}

void HolidayService::EnsureRecentYears(int current_year) {
    // 收集缺失的年份
    std::vector<int> missing;
    for (int y = current_year - 2; y <= current_year; y++) {
        if (!HasYearData(y)) {
            missing.push_back(y);
        }
    }

    if (missing.empty()) return;

    // 后台线程异步获取
    std::thread([this, missing]() {
        for (int year : missing) {
            FetchFromAPI(year);  // 失败静默忽略
        }
    }).detach();
}

DayType HolidayService::GetDayType(int year, int month, int day) const {
    char mmdd[5];
    sprintf_s(mmdd, "%02d%02d", month, day);

    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto year_it = cache_.find(year);
    if (year_it != cache_.end()) {
        auto day_it = year_it->second.find(mmdd);
        if (day_it != year_it->second.end()) {
            if (day_it->second.type == "public_holiday") return DayType::Holiday;
            if (day_it->second.type == "workday") return DayType::Workday;
        }
    }

    return DayType::Normal;
}

std::wstring HolidayService::GetHolidayName(int year, int month, int day) const {
    char mmdd[5];
    sprintf_s(mmdd, "%02d%02d", month, day);

    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto year_it = cache_.find(year);
    if (year_it != cache_.end()) {
        auto day_it = year_it->second.find(mmdd);
        if (day_it != year_it->second.end() && !day_it->second.name.empty()) {
            const std::string& s = day_it->second.name;
            int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
            if (len > 0) {
                std::wstring name(len - 1, L'\0');
                MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &name[0], len);
                return name;
            }
        }
    }
    return L"";
}
