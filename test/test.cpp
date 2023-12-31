#include <cstdio>
#include <string>
#include "fmt/core.h"
#include "fmt/ranges.h"
#include "fmt/xchar.h"
#include <thread>
#include <chrono>
#include <atomic>
#include "exit_event.h"
#include "unicode/wargv.h"
#include "pidmonitor.hpp"


int main(int argc, char** argv) {
    wchar_t** wargv = get_wargv();

    if (argc == 1) {
        std::printf("Usage: pidmon_test.exe [image_name]\n");
    } else {
        if (!init_exit_handler()) {
            std::printf("ERROR: Could not set exit handler!\n");
            return 1;
        }

        const auto session_name = std::wstring_view(L"my_trace_session");
        PidStorageAll pids;
        PidTracer etw_tracer = {session_name};
        for (int i = 1; i < argc; i++) {
            pids.add_image_name(std::wstring_view(wargv[i]));
        }
        pids.subscribe(etw_tracer);
        
        std::atomic_bool run_flag = true;
        auto print_thread = std::thread([&run_flag](PidStorageAll* pids_ptr){
            while ((bool)run_flag) {
                {
                    pids_ptr->get_scope_rlock();
                    fmt::print("{}\n", pids_ptr->get_all_pids());
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        }, &pids);
        wait_for_exit();
        run_flag = false;
        print_thread.join();
    }

    printf("Program exit...\n");
    return 0;
}

