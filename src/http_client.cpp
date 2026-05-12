/**
 * WinHTTP HTTPS GET 请求封装
 * 无额外依赖，直接使用系统 WinHTTP API
 */

#include <windows.h>
#include <winhttp.h>
#include "http_client.h"

#pragma comment(lib, "winhttp.lib")

std::string HttpClient::HttpGet(const std::wstring& server,
                                const std::wstring& path) {
    std::string result;

    HINTERNET hSession = WinHttpOpen(
        L"Lucent/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!hSession) return result;

    // 连接服务器（HTTPS 使用端口 443）
    HINTERNET hConnect = WinHttpConnect(hSession, server.c_str(),
                                         INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return result;
    }

    // 创建 HTTPS 请求
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, L"GET", path.c_str(),
        nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    // 发送请求
    if (!WinHttpSendRequest(hRequest,
                            WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0,
                            0, 0)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    // 接收响应
    if (WinHttpReceiveResponse(hRequest, nullptr)) {
        DWORD dwStatusCode = 0;
        DWORD dwSize = sizeof(dwStatusCode);
        WinHttpQueryHeaders(hRequest,
                            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX,
                            &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);

        if (dwStatusCode == 200) {
            // 读取响应体
            DWORD dwAvailable = 0;
            do {
                dwAvailable = 0;
                if (!WinHttpQueryDataAvailable(hRequest, &dwAvailable)) break;
                if (dwAvailable == 0) break;

                std::vector<char> buffer(dwAvailable + 1);
                DWORD dwRead = 0;
                if (WinHttpReadData(hRequest, buffer.data(), dwAvailable, &dwRead)) {
                    result.append(buffer.data(), dwRead);
                }
            } while (dwAvailable > 0);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}
