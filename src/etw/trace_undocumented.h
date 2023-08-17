#ifdef __TRACE_USE_UNDOCUMENTED__

#ifdef __cplusplus
extern "C" {
#endif

#include "evntbase.h"

// a minimal rip from EventTrace
// thanks to his great effort ;)

#define StartTraceW   __XYZ_StartTraceW
#define ControlTraceW __XYZ_ControlTraceW

ULONG WINAPI __XYZ_StartTraceW(
    PTRACEHANDLE TraceHandle,
    PWSTR InstanceName,
    PEVENT_TRACE_PROPERTIES Properties
);

ULONG WINAPI __XYZ_ControlTraceW(
    TRACEHANDLE TraceHandle,
    PWSTR InstanceName,
    PEVENT_TRACE_PROPERTIES Properties,
    ULONG ControlCode
);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __TRACE_USE_UNDOCUMENTED__
