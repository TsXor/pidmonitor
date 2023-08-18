cmake_minimum_required(VERSION 3.15)

project(pidmonitor)

if (NOT DEFINED PIDMONITOR_ARCH)
    message(STATUS "pidmonitor: PIDMONITOR_ARCH not set, default to x64 ...")
    set(PIDMONITOR_ARCH "x64")
endif()

add_library(pidmonitor STATIC IMPORTED)
set_target_properties(pidmonitor PROPERTIES
    IMPORTED_LOCATION ${CMAKE_CURRENT_LIST_DIR}/${PIDMONITOR_ARCH}/lib/libpidmonitor.a)
