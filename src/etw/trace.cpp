#include <stdexcept>
#include <thread>
#include "winexc.hpp"
#include <handleapi.h>
#include "trace_undocumented.h"
#include "trace.hpp"


namespace EtwTrace {

Session::Session(const std::wstring_view name, const GUID provider_guid,
                 const std::wstring_view logfile_path) {
    if (name.length() >= TRACER_NAME_MAX_LEN) throw std::invalid_argument("tracer name too long");
    
    // set NULLs
    this->_trace_config_ptr = NULL;
    this->_session_handle = INFINITE;

    // if logfile_path is empty string, init without logfile
    bool no_logfile = !logfile_path.length();
    
    // allocate space for trace properties
    size_t config_size = sizeof(EVENT_TRACE_PROPERTIES_WITH_NAMES)
                        + (no_logfile ? 0 : (logfile_path.length() + 1) * sizeof(wchar_t));
    this->_trace_config_ptr = (EVENT_TRACE_PROPERTIES_WITH_NAMES* )malloc(config_size);
    ZeroMemory(this->_trace_config_ptr, config_size);

    // give parameters
    this->_trace_config_ptr->Props.Wnode.BufferSize = sizeof(EVENT_TRACE_PROPERTIES_V2)
                                                    + TRACER_NAME_MAX_LEN * sizeof (wchar_t);
    this->_trace_config_ptr->Props.Wnode.Flags = WNODE_FLAG_TRACED_GUID | WNODE_FLAG_VERSIONED_PROPERTIES;
    this->_trace_config_ptr->Props.LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    this->_trace_config_ptr->Props.LogFileNameOffset = no_logfile ? 0 : offsetof(EVENT_TRACE_PROPERTIES_WITH_NAMES, LogFileName);
    this->_trace_config_ptr->Props.LoggerNameOffset = offsetof(EVENT_TRACE_PROPERTIES_WITH_NAMES, LoggerName);
    // give names
    memcpy(this->_trace_config_ptr->LoggerName, name.data(), name.length() * sizeof(wchar_t));
    this->_trace_config_ptr->LoggerName[name.length()] = L'\0';
    if (!no_logfile) {
        memcpy(this->_trace_config_ptr->LogFileName, logfile_path.data(), logfile_path.length() * sizeof(wchar_t));
        this->_trace_config_ptr->LogFileName[logfile_path.length()] = L'\0';
    }

    // start trace session
    ULONG start_trace_result = StartTraceW( &(this->_session_handle), this->_trace_config_ptr->LoggerName,
        (PEVENT_TRACE_PROPERTIES)&(this->_trace_config_ptr->Props) );
    if (start_trace_result != ERROR_SUCCESS) {
        throw Windows::WinError<char>(start_trace_result, std::string_view("in StartTrace"));
    }

    ULONG enable_trace_result = EnableTraceEx2( this->_session_handle, &provider_guid,
        EVENT_CONTROL_CODE_ENABLE_PROVIDER, TRACE_LEVEL_VERBOSE, 0, 0, 0, NULL );
    if (enable_trace_result != ERROR_SUCCESS) {
        throw Windows::WinError<char>(enable_trace_result, std::string_view("in EnableTraceEx"));
    }
}

Session::~Session() {
    this->_trace_config_ptr->Props.LogFileNameOffset = 0;
    ControlTraceW( this->_session_handle, NULL, 
        (PEVENT_TRACE_PROPERTIES)&(this->_trace_config_ptr->Props), EVENT_TRACE_CONTROL_STOP );
    if (this->_trace_config_ptr) free(this->_trace_config_ptr);
}

Tracer::Tracer(bool session, const std::wstring_view open_name,
               PEVENT_RECORD_CALLBACK callback, PVOID argptr) {
    this->_trace_handle= INVALID_PROCESSTRACE_HANDLE;
    
    // copy name
    this->_name = (wchar_t* )malloc((open_name.length() + 1) * sizeof(wchar_t));
    memcpy(this->_name, open_name.data(), open_name.length() * sizeof(wchar_t));
    this->_name[open_name.length()] = L'\0';
    
    // init from file
    this->_info_ptr = (EVENT_TRACE_LOGFILEW* )malloc(sizeof(EVENT_TRACE_LOGFILEW));
    ZeroMemory(this->_info_ptr, sizeof(EVENT_TRACE_LOGFILEW));
    if (session) {
        this->_info_ptr->LoggerName = this->_name;
        this->_info_ptr->LogFileName = NULL;
    } else {
        this->_info_ptr->LoggerName = NULL;
        this->_info_ptr->LogFileName = this->_name;
    }
    this->_info_ptr->ProcessTraceMode = (session ? PROCESS_TRACE_MODE_REAL_TIME : 0)
                                        | PROCESS_TRACE_MODE_EVENT_RECORD
                                        | PROCESS_TRACE_MODE_RAW_TIMESTAMP;
    this->_info_ptr->EventRecordCallback = callback;
    this->_info_ptr->Context = argptr;

    this->_trace_handle = OpenTraceW(this->_info_ptr);
    if (this->_trace_handle == INVALID_PROCESSTRACE_HANDLE) {
        throw Windows::WinError<char>(GetLastError(), std::string_view("in OpenTrace"));
    }

    auto runner_ptr = new std::thread([](TRACEHANDLE trace_handle){
        ProcessTrace(&trace_handle, 1, NULL, NULL);
    }, this->_trace_handle);
    runner_ptr->detach();
}

Tracer::~Tracer() {
    if (this->_trace_handle != INVALID_PROCESSTRACE_HANDLE)
        CloseTrace(this->_trace_handle);
    free(this->_info_ptr);
    free(this->_name);
}

} // namespace EtwTrace
