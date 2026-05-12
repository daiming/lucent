#pragma once

#include <windows.h>
#include "calendar_model.h"
#include "calendar_renderer.h"
#include "holiday_service.h"
#include "config.h"

// 日历弹出窗口
// visible_ 记录用户意图状态，OnActivate 自动隐藏通过延迟销毁避免与 ToggleShow 竞争
class CalendarWindow {
public:
    CalendarWindow() = default;
    ~CalendarWindow();

    void SetParams(HINSTANCE hInst, HolidayService* holidays);
    bool Create(HINSTANCE hInst, HolidayService* holidays);
    void Show(HWND hRefWnd);
    void Hide();
    void ToggleShow(HWND hRefWnd);
    void GoToday();
    HWND GetHwnd() const { return hwnd_; }

private:
    static void RegisterClass(HINSTANCE hInst);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    LRESULT OnCreate(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT OnPaint(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT OnEraseBkgnd(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT OnNcHitTest(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT OnLButtonDown(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT OnActivate(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT OnTimer(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT OnKeyDown(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT OnDpiChanged(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT OnDestroy(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    void ApplyShadow(HWND hwnd);
    void UpdateModel();
    void NavigateMonth(int delta);

    HWND hwnd_ = nullptr;
    HINSTANCE hInst_ = nullptr;
    HolidayService* holidays_ = nullptr;

    CalendarModel model_;
    CalendarRenderer renderer_;

    int display_year_ = 0;
    int display_month_ = 0;
    int current_dpi_ = 96;
    bool visible_ = false;
    static constexpr UINT kAutoHideTimerId = 1;

    static constexpr const wchar_t* kClassName = L"LucentCalendar";
    static bool class_registered_;
    static HBRUSH bg_brush_;
};
