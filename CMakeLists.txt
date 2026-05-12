cmake_minimum_required(VERSION 3.20)
project(Lucent
    VERSION     1.0.0
    DESCRIPTION "Lightweight Windows taskbar calendar with lunar support"
    LANGUAGES   CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Dependencies ──────────────────────────────────────────────

# tyme4cpp (lunar calendar) - fetch source only
include(FetchContent)
FetchContent_Declare(
  tyme4cpp
  GIT_REPOSITORY https://github.com/6tail/tyme4cpp.git
  GIT_TAG        v1.1.0
)
FetchContent_Populate(tyme4cpp)

# vcpkg packages
find_package(nlohmann_json CONFIG REQUIRED)
# WTL is header-only; vcpkg adds it to the include path automatically

# ── Target ────────────────────────────────────────────────────

add_executable(Lucent WIN32
  src/main.cpp
  src/tray_icon.cpp
  src/calendar_window.cpp
  src/calendar_renderer.cpp
  src/calendar_model.cpp
  src/holiday_service.cpp
  src/http_client.cpp
  src/lunar_provider.cpp
  ${tyme4cpp_SOURCE_DIR}/tyme/tyme.cpp
  ${tyme4cpp_SOURCE_DIR}/tyme/util.cpp
  src/lucent.rc
  src/lucent.manifest
)

target_include_directories(Lucent PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${tyme4cpp_SOURCE_DIR}/tyme
)

target_link_libraries(Lucent PRIVATE
  nlohmann_json::nlohmann_json
  winhttp
  dwmapi
  gdiplus
  comctl32
  shlwapi
)

# ── Compiler / Linker settings ────────────────────────────────

# Static CRT (/MT)
set_property(TARGET Lucent PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded"
)

target_compile_options(Lucent PRIVATE
  /O2           # Maximum optimization
  /GL           # Whole-program optimization
  /utf-8        # Source encoding
  /EHsc         # C++ exceptions
  /Zc:inline    # Remove unreferenced inline functions
  /W4           # High warning level
)

target_compile_definitions(Lucent PRIVATE
  NOMINMAX
  UNICODE
  _UNICODE
)

target_link_options(Lucent PRIVATE
  /LTCG            # Link-time code generation
  /OPT:REF         # Remove unreferenced functions
  /OPT:ICF         # Merge identical COMDAT
  /MERGE:.rdata=.text
)
