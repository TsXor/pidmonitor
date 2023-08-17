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

/**
 * \if en-US
 * @brief class used to update PID storage
 * @note ETW events are handled in a new thread
 * \endif
 *
 * \if zh-CN
 * @brief 用于更新PID存储的类
 * @note ETW事件是在新建的线程中处理的
 * \endif
 */
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

/**
 * \if en-US
 * @brief father class of all PID storage class
 * \endif
 *
 * \if zh-CN
 * @brief 所有PID存储类的父类
 * \endif
 */
class PidStorageBase {
#ifdef __TRACEPID_SELF_INCLUSION__
    friend VOID WINAPI __tracepid_etw_handler_cb(PEVENT_RECORD EventRecord);
#endif
private:
    std::mutex _rwlock;
    virtual void add_pid(const std::wstring_view name_view, win_pid_t pid) = 0;
    // Note: In ProcessStop event, encoding of ImageName is ANSI, and it will be cut to first 14 chars.
    // 注意：退出时ImageName为ANSI编码，且会被裁剪到前14个字符。
    virtual void del_pid(const std::string_view short_name_view, win_pid_t pid) = 0;
public:
    void acquire_lock(void) {this->_rwlock.lock();}
    void release_lock(void) {this->_rwlock.unlock();}
    /**
     * \if en-US
     * @brief bind this PID storage to a tracer
     * @param tracer tracer to bind
     * \endif
     * 
     * \if zh-CN
	 * @brief 将此PID存储绑定到一个跟踪器上
     * @param tracer 要绑定的跟踪器
     * \endif
     */
    void subscribe(PidTracer& tracer);
    /**
     * \if en-US
     * @brief start monitoring an image name
     * @param name_view image name
     * @param init inital PIDs of this image name
     * \endif
     * 
     * \if zh-CN
	 * @brief 开始监控某一映像名
	 * @param name_view 映像名称
     * @param init 该映像名称的初始PID集
     * \endif
     */
    virtual void add_image_name(const std::wstring_view name_view, pid_set_t&& init) = 0;
    /**
     * \if en-US
     * @brief start monitoring an image name
     * @param name_view image name
     * @param init inital PIDs of this image name
     * \endif
     * 
     * \if zh-CN
	 * @brief 开始监控某一映像名
	 * @param name_view 映像名称
     * @param init 该映像名称的初始PID集
     * \endif
     */
    virtual void add_image_name(const std::wstring_view name_view, const pid_set_t& init) = 0;
    /**
     * \if en-US
     * @brief start monitoring an image name
     * @param name_view image name
     * @param init_fn a function to get inital PIDs of this image name
     * \endif
     * 
     * \if zh-CN
	 * @brief 开始监控某一映像名
	 * @param name_view 映像名称
     * @param init_fn 用于获取该映像名称的初始PID集的函数
     * \endif
     */
    virtual void add_image_name(const std::wstring_view name_view, pid_init_fn_t init_fn) = 0;
    /**
     * \if en-US
     * @brief stop monitoring an image name
     * @param name_view image name
     * \endif
     * 
     * \if zh-CN
	 * @brief 停止监控某一映像名
	 * @param name_view 映像名称
     * \endif
     */
    virtual void del_image_name(const std::wstring_view name_view) = 0;
};

/**
 * \if en-US
 * @brief store PID separated according to image name
 * @note when you need to check if a PID is in any of the image name you monitored, you need to traverse all the sets
 * \endif
 *
 * \if zh-CN
 * @brief 依照映像名称将PID分开存储
 * @note 如果你需要检查一个PID是否为任何一个你监控的映像名所有，你需要遍历所有集合
 * \endif
 */
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

/**
 * \if en-US
 * @brief store all PIDs in a single set
 * \endif
 *
 * \if zh-CN
 * @brief 将所有PID存储在同一个集合中
 * \endif
 */
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
