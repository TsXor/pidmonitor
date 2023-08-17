#pragma once
#ifndef __ETW_TRACE_HPP__
#define __ETW_TRACE_HPP__

#include <string>
#include "evntbase.h"


#ifndef INFINITE
#define INFINITE 0xffffffff
#endif

// for more, see https://github.com/Biswa96/TraceEvent/blob/master/docs/Event_Providers.md
static const GUID MSNT_SystemTrace = {0x9E814AAD, 0x3204, 0x11D2, {0x9A, 0x82, 0x00, 0x60, 0x08, 0xA8, 0x69, 0x39}};

static const GUID MemoryProvGuid       = {0xD1D93EF7, 0xE1F2, 0x4F45, {0x99, 0x43, 0x03, 0xD2, 0x45, 0xFE, 0x6C, 0x00}};
static const GUID FileProvGuid         = {0xEDD08927, 0x9CC4, 0x4E65, {0xB9, 0x70, 0xC2, 0x56, 0x0F, 0xB5, 0xC2, 0x89}};
static const GUID DiskProvGuid         = {0xC7BDE69A, 0xE1E0, 0x4177, {0xB6, 0xEF, 0x28, 0x3A, 0xD1, 0x52, 0x52, 0x71}};
static const GUID NetProvGuid          = {0x7DD42A49, 0x5329, 0x4832, {0x8D, 0xFD, 0x43, 0xD9, 0x79, 0x15, 0x3A, 0x88}};
static const GUID PsProvGuid           = {0x22FB2CD6, 0x0E7B, 0x422B, {0xA0, 0xC7, 0x2F, 0xAD, 0x1F, 0xD0, 0xE7, 0x16}};
static const GUID KernelProvGuid       = {0xA68CA8B7, 0x004F, 0xD7B6, {0xA6, 0x98, 0x07, 0xE2, 0xDE, 0x0F, 0x1F, 0x5D}};
static const GUID EventTracingProvGuid = {0xB675EC37, 0xBDB6, 0x4648, {0xBC, 0x92, 0xF3, 0xFD, 0xC7, 0x4D, 0x3C, 0xA2}};


// Reference: https://www.cnblogs.com/Icys/p/EtwProcess.html
namespace EtwTrace {

#define TRACER_NAME_MAX_LEN 256

typedef struct {
    EVENT_TRACE_PROPERTIES_V2 Props;
    wchar_t LoggerName[TRACER_NAME_MAX_LEN];
    wchar_t LogFileName[];
} EVENT_TRACE_PROPERTIES_WITH_NAMES;


class Session {
private:
    EVENT_TRACE_PROPERTIES_WITH_NAMES* _trace_config_ptr;
    TRACEHANDLE _session_handle;
public:
    Session(const std::wstring_view name, const GUID provider_guid,
            const std::wstring_view logfile_path = std::wstring_view(L"") );
    ~Session();
};

class Tracer {
private:
    wchar_t* _name;
    EVENT_TRACE_LOGFILEW* _info_ptr;
    TRACEHANDLE _trace_handle;
public:
    Tracer(bool session, const std::wstring_view open_name,
           PEVENT_RECORD_CALLBACK callback, PVOID argptr = NULL);
    ~Tracer();
};

} // namespace EtwTrace

#endif // __ETW_TRACE_HPP__
