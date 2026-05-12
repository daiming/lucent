/**
 * Lucent - Windows 任务栏日历工具
 * 程序入口点
 */

#include <windows.h>
#include "app.h"

int WINAPI WinMain(_In_ HINSTANCE hInst,
                   _In_opt_ HINSTANCE,
                   _In_ LPSTR,
                   _In_ int) {
    CApp app;
    if (!app.Init(hInst)) {
        return 1;
    }
    int ret = app.Run();
    app.Quit();
    return ret;
}
