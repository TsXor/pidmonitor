cmake_minimum_required(VERSION 3.15)

project(pidmonitor)


# use c++20 for concept/requires
set(CMAKE_CXX_STANDARD 20)
# hide symbols in libs by default
set(CMAKE_CXX_VISIBILITY_PRESET hidden)


include(build/cmake-windows-utils/CMakeLists.txt)

# import Windows dlls
import_extern_dll(ole ole32 MS_BUILTIN)
import_extern_dll(oleauto oleaut32 MS_BUILTIN)

# import fmtlib
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/extlibs/fmt fmtlib.out EXCLUDE_FROM_ALL)

# import windows-bricks
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/extlibs/windows-bricks windows-bricks.out EXCLUDE_FROM_ALL)


# name2pid_wmi
add_library(name2pid STATIC ${CMAKE_CURRENT_LIST_DIR}/src/wmi/name2pid.cpp)
target_link_libraries(name2pid ole)
target_link_libraries(name2pid oleauto)
target_link_libraries(name2pid wbemuuid)
target_include_directories(name2pid PRIVATE ${CMAKE_CURRENT_LIST_DIR}/extlibs/windows-bricks/include)
target_link_libraries(name2pid fmt::fmt)
target_include_directories(name2pid PRIVATE ${CMAKE_CURRENT_LIST_DIR}/extlibs/fmt/include)

# trace_undocumented
add_library(trace_undocumented STATIC ${CMAKE_CURRENT_LIST_DIR}/src/etw/trace_undocumented.c)
target_link_libraries(trace_undocumented ntdll)

# tracepid_etw
add_library(tracepid STATIC src/etw/tracepid.cpp ${CMAKE_CURRENT_LIST_DIR}/src/etw/trace.cpp)
target_link_libraries(tracepid trace_undocumented)
target_link_libraries(tracepid name2pid)
target_include_directories(tracepid PRIVATE ${CMAKE_CURRENT_LIST_DIR}/extlibs/windows-bricks/include)

# pidmon_test
add_executable(pidmon_test WIN32 test/test.cpp)
target_use_console_subsystem(pidmon_test)
target_uac_admin(pidmon_test)
target_include_directories(pidmon_test PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(pidmon_test exit_event)
target_include_directories(pidmon_test PRIVATE ${CMAKE_CURRENT_LIST_DIR}/extlibs/windows-bricks/include)
target_link_libraries(pidmon_test name2pid)
target_link_libraries(pidmon_test tracepid)


# install
file(GLOB pidmon_headers  "${CMAKE_CURRENT_LIST_DIR}/include/*")
install(FILES ${pidmon_headers}
        DESTINATION ${CMAKE_CURRENT_LIST_DIR}/dist/include)
install(TARGETS name2pid
        DESTINATION ${CMAKE_CURRENT_LIST_DIR}/dist/lib/${ARCH})
install(TARGETS tracepid
        DESTINATION ${CMAKE_CURRENT_LIST_DIR}/dist/lib/${ARCH})
install(TARGETS pidmon_test
        DESTINATION ${CMAKE_CURRENT_LIST_DIR}/dist/bin/${ARCH})
