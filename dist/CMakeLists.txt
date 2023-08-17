cmake_minimum_required(VERSION 3.15)

project(pidmonitor)

if (NOT DEFINED PIDMONITOR_ARCH)
    message(STATUS "pidmonitor: PIDMONITOR_ARCH not set, default to x64 ...")
    set(PIDMONITOR_ARCH "x64")
endif()

add_library(name2pid STATIC IMPORTED)
set_target_properties(name2pid PROPERTIES
    IMPORTED_LOCATION ${CMAKE_CURRENT_LIST_DIR}/${PIDMONITOR_ARCH}/lib/libname2pid.a)
add_library(tracepid STATIC IMPORTED)
set_target_properties(tracepid PROPERTIES
    IMPORTED_LOCATION ${CMAKE_CURRENT_LIST_DIR}/${PIDMONITOR_ARCH}/lib/libtracepid.a)
