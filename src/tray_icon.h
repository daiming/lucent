#pragma once

#include <windows.h>
#include <shellapi.h>
#include <string>

// 系统托盘图标管理
class TrayIcon {
public:
    TrayIcon() = default;
    ~TrayIcon() { Remove(); }

    // 创建托盘图标
    // hOwner: 接收回调消息的窗口句柄
    // callbackMsg: 回调消息 ID
    bool Create(HWND hOwner, UINT callbackMsg);

    // 移除托盘图标
    void Remove();

    // 更新图标
    void UpdateIcon(HICON hIcon);

    // 更新提示文本
    void UpdateTooltip(const std::wstring& text);

    // 动态生成带日期数字的图标
    static HICON GenerateDateIcon(int day, int dpi);

private:
    NOTIFYICONDATAW nid_ = {};
    bool created_ = false;
};
