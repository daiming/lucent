# Lucent

轻量级 Windows 任务栏日历工具，支持公历、农历、节假日、调休补班标记，极低内存占用，空闲时仅占用3Mb左右内存。纯粹无广告！

## 功能特性

- **系统托盘** — 托盘图标显示，左键弹出日历窗口
- **公历 + 农历** — 每日显示公历日期与农历/节日文本
- **节假日标记** — 通过 API 自动获取国家法定节假日数据，绿色背景 + "休" 角标
- **调休补班** — 补班日红色背景 + "班" 角标
- **今日高亮** — 蓝色背景标记当天，支持日期选中
- **月份切换** — 左右方向键或按钮切换月份，Home 键回到今天
- **高 DPI** — Per-Monitor DPI V2 感知，自动缩放
- **开机启动** — 右键菜单可设置/取消随系统启动
- **无边框阴影** — DWM 原生阴影 + Win11 圆角

## 环境要求

| 工具 | 版本 |
|------|------|
| Visual Studio | 2022 (MSVC) |
| CMake | >= 3.20 |
| vcpkg | 最新版 |

## 前置依赖

通过 vcpkg 安装：

```bash
vcpkg install wtl nlohmann-json
```

如需全局集成（推荐）：

```bash
vcpkg integrate install
```

## 编译

```bash
# 1. 配置（需指定 vcpkg 工具链）
cmake -B build -G "Visual Studio 17 2022" -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake

# 2. 编译 Release
cmake --build build --config Release

# 3. 运行
build/Release/Lucent.exe
```

### Visual Studio 中打开

```bash
cmake -B build -G "Visual Studio 17 2022" ^
    -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake
```

然后打开 `build/Lucent.sln`。

## 项目结构

```
Lucent/
├── CMakeLists.txt          # 构建配置
├── vcpkg.json              # vcpkg 依赖声明
├── assets/
│   └── lucent.ico          # 应用图标
├── src/
│   ├── main.cpp            # WinMain 入口
│   ├── app.h               # CApp 应用生命周期管理
│   ├── config.h            # 常量、颜色、DPI 工具函数
│   ├── resource.h          # 资源 ID 定义
│   ├── lucent.rc           # Windows 资源文件（图标、清单）
│   ├── lucent.manifest     # DPI 感知 + 公共控件 v6 清单
│   ├── tray_icon.h/.cpp    # 系统托盘图标
│   ├── calendar_window.h/.cpp  # 日历弹出窗口（DWM 阴影、拖拽）
│   ├── calendar_renderer.h/.cpp # GDI/GDI+ 绘制引擎
│   ├── calendar_model.h/.cpp    # 6×7 日历数据模型
│   ├── holiday_service.h/.cpp   # 节假日 JSON 缓存 + API 获取
│   ├── http_client.h/.cpp       # WinHTTP 封装
│   └── lunar_provider.h/.cpp    # tyme4cpp 农历库适配器
└── docs/                   # 设计文档
```

## 依赖说明

| 依赖 | 来源 | 说明 |
|------|------|------|
| nlohmann/json | vcpkg | JSON 解析（节假日缓存） |
| tyme4cpp | CMake FetchContent | 农历计算（v1.1.0） |
| WTL | vcpkg | 头文件库（当前未使用模板类） |
| WinHTTP | 系统库 | HTTPS 请求获取节假日数据 |
| DWM API | 系统库 | 窗口阴影和圆角 |
| GDI+ | 系统库 | 抗锯齿圆角矩形绘制 |

## 运行时数据

应用启动时自动创建 `%APPDATA%\Lucent\data\` 目录：

- `holidays.json` — 节假日缓存，首次运行从 `timor.tech` API 拉取当年数据

## 开发说明

### 添加新的颜色/常量

编辑 `src/config.h` 中的 `cfg` 命名空间。

### 添加新的菜单项

1. 在 `src/resource.h` 添加 `IDM_*` 定义
2. 在 `src/app.h` 的 `OnCommand` 中处理
3. 在 `ShowContextMenu` 中添加菜单项

### 窗口截图

![image](docs/ScreenShot1.png)

## License

MIT
