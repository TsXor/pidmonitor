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


# trace_undocumented
#add_library(trace_undocumented STATIC ${CMAKE_CURRENT_LIST_DIR}/src/etw/trace_undocumented.c)
#target_link_libraries(trace_undocumented ntdll)

# pidmonitor
add_library(pidmonitor STATIC ${CMAKE_CURRENT_LIST_DIR}/src/etw/pidmonitor.cpp
                              ${CMAKE_CURRENT_LIST_DIR}/src/etw/trace.cpp
                              ${CMAKE_CURRENT_LIST_DIR}/src/wmi/name2pid.cpp)
#target_link_libraries(pidmonitor trace_undocumented)
target_include_directories(pidmonitor PRIVATE ${CMAKE_CURRENT_LIST_DIR}/extlibs/windows-bricks/include)
target_include_directories(pidmonitor PRIVATE ${CMAKE_CURRENT_LIST_DIR}/extlibs/fmt/include)
target_link_libraries(pidmonitor ole)
target_link_libraries(pidmonitor oleauto)
target_link_libraries(pidmonitor wbemuuid)
target_link_libraries(pidmonitor fmt::fmt)

# pidmon_test
add_executable(pidmon_test WIN32 test/test.cpp)
target_use_console_subsystem(pidmon_test)
target_uac_admin(pidmon_test)
target_link_libraries(pidmon_test exit_event)
target_include_directories(pidmon_test PRIVATE ${CMAKE_CURRENT_LIST_DIR}/extlibs/windows-bricks/include)
target_link_libraries(pidmon_test pidmonitor)
target_include_directories(pidmon_test PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)


# install
file(GLOB pidmon_headers "${CMAKE_CURRENT_LIST_DIR}/include/*")
install(FILES ${pidmon_headers}
        DESTINATION ${CMAKE_CURRENT_LIST_DIR}/dist/include)
install(TARGETS pidmonitor
        DESTINATION ${CMAKE_CURRENT_LIST_DIR}/dist/lib/${ARCH})
install(TARGETS pidmon_test
        DESTINATION ${CMAKE_CURRENT_LIST_DIR}/dist/bin/${ARCH})
