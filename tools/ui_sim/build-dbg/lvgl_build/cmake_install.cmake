# Install script for directory: /storage/Code/j6p_0330/Sample/Claude/AgentIndicator/firmware/managed_components/lvgl__lvgl

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/lvgl" TYPE DIRECTORY FILES "/storage/Code/j6p_0330/Sample/Claude/AgentIndicator/firmware/managed_components/lvgl__lvgl/src" FILES_MATCHING REGEX "/[^/]*\\.h$" REGEX "/[^/]*\\_private\\.h$" EXCLUDE)
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/lvgl" TYPE FILE FILES
    "/storage/Code/j6p_0330/Sample/Claude/AgentIndicator/firmware/managed_components/lvgl__lvgl/lv_version.h"
    "/storage/Code/j6p_0330/Sample/Claude/AgentIndicator/firmware/managed_components/lvgl__lvgl/lvgl.h"
    "/storage/Code/j6p_0330/Sample/Claude/AgentIndicator/tools/ui_sim/lv_conf.h"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/storage/Code/j6p_0330/Sample/Claude/AgentIndicator/tools/ui_sim/build-dbg/lvgl_build/lib/liblvgl.a")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/lvgl" TYPE FILE FILES
    "/storage/Code/j6p_0330/Sample/Claude/AgentIndicator/firmware/managed_components/lvgl__lvgl/lv_version.h"
    "/storage/Code/j6p_0330/Sample/Claude/AgentIndicator/firmware/managed_components/lvgl__lvgl/lvgl.h"
    "/storage/Code/j6p_0330/Sample/Claude/AgentIndicator/tools/ui_sim/lv_conf.h"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/pkgconfig" TYPE FILE FILES "/storage/Code/j6p_0330/Sample/Claude/AgentIndicator/tools/ui_sim/build-dbg/lvgl_build/lvgl.pc")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/storage/Code/j6p_0330/Sample/Claude/AgentIndicator/tools/ui_sim/build-dbg/lvgl_build/lib/liblvgl_thorvg.a")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/lvgl" TYPE FILE FILES
    "/storage/Code/j6p_0330/Sample/Claude/AgentIndicator/firmware/managed_components/lvgl__lvgl/lv_version.h"
    "/storage/Code/j6p_0330/Sample/Claude/AgentIndicator/firmware/managed_components/lvgl__lvgl/lvgl.h"
    "/storage/Code/j6p_0330/Sample/Claude/AgentIndicator/tools/ui_sim/lv_conf.h"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/lvgl" TYPE DIRECTORY FILES "/storage/Code/j6p_0330/Sample/Claude/AgentIndicator/firmware/managed_components/lvgl__lvgl/demos" FILES_MATCHING REGEX "/[^/]*\\.h$")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/storage/Code/j6p_0330/Sample/Claude/AgentIndicator/tools/ui_sim/build-dbg/lvgl_build/lib/liblvgl_demos.a")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/lvgl" TYPE FILE FILES
    "/storage/Code/j6p_0330/Sample/Claude/AgentIndicator/firmware/managed_components/lvgl__lvgl/lv_version.h"
    "/storage/Code/j6p_0330/Sample/Claude/AgentIndicator/firmware/managed_components/lvgl__lvgl/lvgl.h"
    "/storage/Code/j6p_0330/Sample/Claude/AgentIndicator/tools/ui_sim/lv_conf.h"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/lvgl" TYPE DIRECTORY FILES "/storage/Code/j6p_0330/Sample/Claude/AgentIndicator/firmware/managed_components/lvgl__lvgl/examples" FILES_MATCHING REGEX "/[^/]*\\.h$")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/storage/Code/j6p_0330/Sample/Claude/AgentIndicator/tools/ui_sim/build-dbg/lvgl_build/lib/liblvgl_examples.a")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/lvgl" TYPE FILE FILES
    "/storage/Code/j6p_0330/Sample/Claude/AgentIndicator/firmware/managed_components/lvgl__lvgl/lv_version.h"
    "/storage/Code/j6p_0330/Sample/Claude/AgentIndicator/firmware/managed_components/lvgl__lvgl/lvgl.h"
    "/storage/Code/j6p_0330/Sample/Claude/AgentIndicator/tools/ui_sim/lv_conf.h"
    )
endif()

