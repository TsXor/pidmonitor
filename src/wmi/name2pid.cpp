// Windows COM things must be jailed before the binary wall...

#include "fmt/core.h"
#include "fmt/xchar.h"
#include "unicode/wargv.h"
#include "query.hpp"
#include "../../include/name2pid.hpp"

// backend used to query WMI imformation
static WMIQuery::Backend __wmiqbk;

std::unordered_set<win_pid_t> NameToPidWmiEx(const std::wstring_view& procname,
                                             const std::wstring_view& interpreted) {
    bool check_interpreted = !interpreted.empty();
    std::unordered_set<win_pid_t> ret;
    std::wstring query = fmt::format(L"select CommandLine, ProcessId from Win32_Process where Name='{}'", procname);
    WMIQuery::ResultProperty prop_cmdline; WMIQuery::ResultProperty prop_pid;
    __wmiqbk.wql_query(query).traverse(
        [&ret, &prop_cmdline, &prop_pid, &interpreted, check_interpreted]
        (WMIQuery::ResultObject&& result) {
            result.get_property(L"ProcessId", prop_pid);
            win_pid_t pid = prop_pid.data().ulVal;
            if (!check_interpreted) {
                ret.emplace(pid); return;
            }
            
            result.get_property(L"CommandLine", prop_cmdline);
            const wchar_t* cmdline = prop_cmdline.data().bstrVal;
            // I know this is less efficient, but it is more persistent.
            int wargc; wchar_t** wargv = CommandLineToArgvW(cmdline, &wargc);
            for (size_t i = 1; i < wargc; i++) {
                if (std::wstring_view(wargv[i]) == interpreted) {
                    ret.emplace(pid); break;
                }
            }
            LocalFree(wargv);
        }
    );
    return ret;
}

extern std::unordered_set<win_pid_t> NameToPidWmi(const std::wstring_view& procname) {
    return NameToPidWmiEx(procname, std::wstring_view(L""));
}
