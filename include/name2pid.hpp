#pragma once
#ifndef __NAME2PID_HPP__
#define __NAME2PID_HPP__


#ifndef _WIN_PID_T_
#define _WIN_PID_T_
typedef unsigned long win_pid_t;
#endif // _WIN_PID_T_

#include <unordered_set>
#include <string>


/**
 *  @brief The *PLUS* version. You can also look up executables of
 *  interpreted langs with it.
 *
 *  @param procname
 *         name of process, give std::wstring_view(L"*") to match any
 * 
 *  @param interpreted
 *         name of interpreted file
 *
 *  @return an unordered_set that contains all PIDs of required name
 */
extern std::unordered_set<win_pid_t> NameToPidWmiEx(const std::wstring_view& procname,
                                                    const std::wstring_view& interpreted);

/**
 *  @brief Query all PIDs of a process name with WMI.
 *
 *  @param procname
 *         name of process
 *
 *  @return an unordered_set that contains all PIDs of required name
 */
extern std::unordered_set<win_pid_t> NameToPidWmi(const std::wstring_view& procname);

#endif // __NAME2PID_HPP__
