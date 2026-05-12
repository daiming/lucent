#pragma once

#include <string>
#include <vector>

// WinHTTP 封装 - HTTPS GET 请求
class HttpClient {
public:
    // 发送 HTTPS GET 请求，返回响应体
    // server: 域名，如 L"timor.tech"
    // path: 路径，如 L"/api/holiday/year/2026/"
    static std::string HttpGet(const std::wstring& server,
                               const std::wstring& path);
};
