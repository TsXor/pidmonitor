#pragma once
#ifndef __TRACEPID_HPP__
#define __TRACEPID_HPP__

#include <string>
#include <unordered_set>
#include <mutex>

#ifndef _WIN_PID_T_
#define _WIN_PID_T_
typedef unsigned long win_pid_t;
#endif // _WIN_PID_T_

using pid_set_t = std::unordered_set<win_pid_t>;
typedef pid_set_t (*pid_init_fn_t)(const std::wstring_view&);

#ifdef __TRACEPID_SELF_INCLUSION__
static VOID WINAPI __tracepid_etw_handler_cb(PEVENT_RECORD EventRecord);
#endif

class PidStorageBase;

class PidTracer {
#ifdef __TRACEPID_SELF_INCLUSION__
    friend VOID WINAPI __tracepid_etw_handler_cb(PEVENT_RECORD EventRecord);
#endif
    friend class PidStorageBase;
private:
    class Data;
    Data* _data;
public:
    PidTracer(const std::wstring_view name);
    ~PidTracer();
};

class PidStorageBase {
#ifdef __TRACEPID_SELF_INCLUSION__
    friend VOID WINAPI __tracepid_etw_handler_cb(PEVENT_RECORD EventRecord);
#endif
private:
    std::mutex _rwlock;
    virtual void add_pid(const std::wstring_view name_view, win_pid_t pid) = 0;
    // 注意：退出时ImageName为ANSI编码，且会被裁剪到前14个字符
    virtual void del_pid(const std::string_view short_name_view, win_pid_t pid) = 0;
public:
    void acquire_lock(void) {this->_rwlock.lock();}
    void release_lock(void) {this->_rwlock.unlock();}
    void subscribe(PidTracer& tracer);
    virtual void add_image_name(const std::wstring_view name_view, pid_set_t&& init) = 0;
    virtual void add_image_name(const std::wstring_view name_view, const pid_set_t& init) = 0;
    virtual void add_image_name(const std::wstring_view name_view, pid_init_fn_t init_fn) = 0;
    virtual void del_image_name(const std::wstring_view name_view) = 0;
};

class PidStoragePerImage : public PidStorageBase {
private:
    class Data;
    Data* _data;
    void add_pid(const std::wstring_view name_view, win_pid_t pid);
    void del_pid(const std::string_view short_name_view, win_pid_t pid);
public:
    PidStoragePerImage();
    ~PidStoragePerImage();
    void add_image_name(const std::wstring_view name_view, pid_set_t&& init);
    void add_image_name(const std::wstring_view name_view, const pid_set_t& init) {
        // copy and move
        this->add_image_name(name_view, std::move(pid_set_t(init)));
    }
    void add_image_name(const std::wstring_view name_view, pid_init_fn_t init_fn) {
        this->add_image_name(name_view, init_fn(name_view));
    }
    void del_image_name(const std::wstring_view name_view);
    const pid_set_t& get_image_pids(const std::wstring_view name_view);
};

class PidStorageAll : public PidStorageBase {
private:
    class Data;
    Data* _data;
    void add_pid(const std::wstring_view name_view, win_pid_t pid);
    void del_pid(const std::string_view short_name_view, win_pid_t pid);
public:
    PidStorageAll();
    ~PidStorageAll();
    void add_image_name(const std::wstring_view name_view, pid_set_t&& init);
    void add_image_name(const std::wstring_view name_view, const pid_set_t& init) {
        // copy and move
        this->add_image_name(name_view, std::move(pid_set_t(init)));
    }
    void add_image_name(const std::wstring_view name_view, pid_init_fn_t init_fn) {
        this->add_image_name(name_view, init_fn(name_view));
    }
    void del_image_name(const std::wstring_view name_view);
    const pid_set_t& get_all_pids(void);
};

#endif // __TRACEPID_HPP__
