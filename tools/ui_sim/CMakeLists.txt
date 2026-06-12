# Agent Indicator UI 模拟器:在 PC 上无头渲染固件同款界面并导出截图。
# 复用 firmware/managed_components/lvgl__lvgl(先编译一次固件以拉取组件),
# 或 -DLVGL_DIR=<path> 指定任意 lvgl v9.x 源码目录。
#
#   cmake -B build && cmake --build build -j
#   ./build/ui_sim out_dir/      # 输出 ui-*.bmp
cmake_minimum_required(VERSION 3.16)
project(ui_sim C)

set(CMAKE_C_STANDARD 11)

if(NOT DEFINED LVGL_DIR)
    set(LVGL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../firmware/managed_components/lvgl__lvgl)
endif()
if(NOT EXISTS ${LVGL_DIR}/lvgl.h)
    message(FATAL_ERROR "LVGL not found at ${LVGL_DIR}; build firmware once or pass -DLVGL_DIR")
endif()

# LVGL 配置在本目录 lv_conf.h(lvgl 桌面构建自动从顶层工程目录读取)
add_subdirectory(${LVGL_DIR} lvgl_build EXCLUDE_FROM_ALL)

set(SCREENS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../firmware/main/ui/screens)
add_executable(ui_sim
    main.c
    ${SCREENS_DIR}/screens.c
    ${SCREENS_DIR}/screen_wifi.c
    ${SCREENS_DIR}/screen_devices.c
    ${SCREENS_DIR}/screen_files.c
    ${SCREENS_DIR}/screen_music.c
    ${SCREENS_DIR}/screen_light.c
)
target_include_directories(ui_sim PRIVATE ${SCREENS_DIR} ${LVGL_DIR})
target_link_libraries(ui_sim lvgl m)
