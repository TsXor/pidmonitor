#ifdef __TRACE_USE_UNDOCUMENTED__

#include <Windows.h>
#include <strsafe.h>
#include "trace_undocumented.h"


// a minimal rip from EventTrace
// thanks to his great effort ;)

// fix:
//     a typo
//     segfault in ControlTraceW


#define EVENT_TRACE_CLOCK_RAW 0
#define EVENT_TRACE_CLOCK_PERFCOUNTER 1
#define EVENT_TRACE_CLOCK_SYSTEMTIME 2
#define EVENT_TRACE_CLOCK_CPUCYCLE 3


static const GUID CKCLGuid = {0x54DEA73A, 0xED1F, 0x42A4, {0xAF, 0x71, 0x3E, 0x63, 0xD0, 0x56, 0xF1, 0x74}};
static const GUID SystemTraceControl = {0x9E814AAD, 0x3204, 0x11D2, {0x9A, 0x82, 0x00, 0x60, 0x08, 0xA8, 0x69, 0x39}};


// Hand-made from ntoskrnl!NtTraceControl
typedef enum _TRACE_CONTROL_FUNCTION_CLASS {
    TraceControlStartLogger = 1,
    TraceControlStopLogger = 2,
    TraceControlQueryLogger = 3,
    TraceControlUpdateLogger = 4,
    TraceControlFlushLogger = 5,
    TraceControlIncrementLoggerFile = 6,

    TraceControlRealtimeConnect = 11,
    TraceControlWdiDispatchControl = 13,
    TraceControlRealtimeDisconnectConsumerByHandle = 14,

    TraceControlReceiveNotification = 16,
    TraceControlEnableGuid = 17, // TraceControlNotifyGuid
    TraceControlSendReplyDataBlock = 18,
    TraceControlReceiveReplyDataBlock = 19,
    TraceControlWdiUpdateSem = 20,
    TraceControlGetTraceGuidList = 21,
    TraceControlGetTraceGuidInfo = 22,
    TraceControlEnumerateTraceGuids = 23,
    // 24
    TraceControlQueryReferenceTime = 25,
    TraceControlTrackProviderBinary = 26,
    TraceControlAddNotificationEvent = 27,
    TraceControlUpdateDisallowList = 28,

    TraceControlUseDescriptorTypeUm = 31,
    TraceControlGetTraceGroupList = 32,
    TraceControlGetTraceGroupInfo = 33,
    TraceControlTraceSetDisallowList= 34,
    TraceControlSetCompressionSettings = 35,
    TraceControlGetCompressionSettings= 36,
    TraceControlUpdatePeriodicCaptureState = 37,
    TraceControlGetPrivateSessionTraceHandle = 38,
    TraceControlRegisterPrivateSession = 39,
    TraceControlQuerySessionDemuxObject = 40,
    TraceControlSetProviderBinaryTracking = 41,
    TraceControlMaxLoggers = 42,
    TraceControlMaxPmcCounter = 43
} TRACE_CONTROL_FUNCTION_CLASS;

typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

typedef struct _UNICODE_STRING64 {
    USHORT Length;
    USHORT MaximumLength;
    ULONG64 Buffer;
} UNICODE_STRING64, *PUNICODE_STRING64;

//Extracted from ComBase.pdb symbol file
typedef struct _WMI_LOGGER_INFORMATION {
    WNODE_HEADER Wnode;
    ULONG BufferSize;
    ULONG MinimumBuffers;
    ULONG MaximumBuffers;
    ULONG MaximumFileSize;
    ULONG LogFileMode;
    ULONG FlushTimer;
    ULONG EnableFlags;
    union {
        LONG AgeLimit;
        LONG FlushThreshold;
    };
    union {
        struct {
            ULONG Wow : 1;
            ULONG QpcDeltaTracking : 1;
        };
        ULONG64 V2Options;
    };
    union {
        HANDLE LogFileHandle;
        ULONG64 LogFileHandle64;
    };
    union {
        ULONG NumberOfBuffers;
        ULONG InstanceCount;
    };
    union {
        ULONG FreeBuffers;
        ULONG InstanceId;
    };
    union {
        ULONG EventsLost;
        ULONG NumberOfProcessors;
    };
    ULONG BuffersWritten;
    union {
        ULONG LogBuffersLost;
        ULONG Flags;
    };
    ULONG RealTimeBuffersLost;
    union {
        HANDLE LoggerThreadId;
        ULONG64 LoggerThreadId64;
    };
    union {
        UNICODE_STRING LogFileName;
        UNICODE_STRING64 LogFileName64;
    };
    union {
        UNICODE_STRING LoggerName;
        UNICODE_STRING64 LoggerName64;
    };
    ULONG RealTimeConsumerCount;
    ULONG SequenceNumber;
    union {
        PVOID LoggerExtension;
        ULONG64 LoggerExtension64;
    };
} WMI_LOGGER_INFORMATION, *PWMI_LOGGER_INFORMATION;

extern void NTAPI RtlSetLastWin32Error(ULONG LastError);
extern void NTAPI RtlInitUnicodeString(PUNICODE_STRING DestinationString, PCWSTR SourceString);
extern ULONG NTAPI RtlNtStatusToDosError(NTSTATUS Status);
// From ProcessHacker phnt/include/ntmisc.h
extern NTSTATUS NTAPI NtTraceControl(TRACE_CONTROL_FUNCTION_CLASS FunctionCode, PVOID InBuffer,
                                     ULONG InBufferLen, PVOID OutBuffer,
                                     ULONG OutBufferLen, PULONG ReturnLength);

static ULONG WINAPI EtwpValidateTraceProperties(PEVENT_TRACE_PROPERTIES_V2 Properties,
                                         PULONG pFilterDescCount,
                                         PEVENT_FILTER_DESCRIPTOR* pFilterDesc,
                                         PULONG ReturnedLength) {
    
    if (Properties->Wnode.BufferSize < sizeof(EVENT_TRACE_PROPERTIES))
        return ERROR_BAD_LENGTH;

    ULONG LoggerNameOffset = Properties->LoggerNameOffset;
    if (LoggerNameOffset && (LoggerNameOffset < sizeof(EVENT_TRACE_PROPERTIES) ||
        LoggerNameOffset > Properties->Wnode.BufferSize))
        return ERROR_INVALID_PARAMETER;

    ULONG LogFileNameOffset = Properties->LogFileNameOffset;
    if (LogFileNameOffset) {
        if (LogFileNameOffset < sizeof(EVENT_TRACE_PROPERTIES) ||
            LogFileNameOffset > Properties->Wnode.BufferSize)
            return ERROR_INVALID_PARAMETER;
    }

    if (!(Properties->Wnode.Flags & WNODE_FLAG_VERSIONED_PROPERTIES)) {
        *ReturnedLength = sizeof(EVENT_TRACE_PROPERTIES);
        return ERROR_SUCCESS;
    }

    *ReturnedLength = sizeof(EVENT_TRACE_PROPERTIES_V2);
    if (Properties->Wnode.BufferSize < sizeof(EVENT_TRACE_PROPERTIES_V2))
        return ERROR_BAD_LENGTH;

    PEVENT_FILTER_DESCRIPTOR FilterDesc = Properties->FilterDesc;
    ULONG FilterDescCount = Properties->FilterDescCount;
    if ((FilterDescCount == 0) != (FilterDesc == NULL))
        return ERROR_INVALID_PARAMETER;

    if (!FilterDescCount ||
        !(Properties->LogFileMode & EVENT_TRACE_PRIVATE_LOGGER_MODE))
        return ERROR_SUCCESS;
    // EtwpValidateFilterDescriptors(TRUE, 0, FilterDescCount, FilterDesc)

    *pFilterDescCount = FilterDescCount;
    *pFilterDesc = FilterDesc;
    return ERROR_SUCCESS;
}

static void WINAPI EtwpCopyPropertiesToInfo(PEVENT_TRACE_PROPERTIES_V2 Properties,
                                     PWMI_LOGGER_INFORMATION WmiLogInfo) {
    
    WmiLogInfo->Wnode.BufferSize = Properties->Wnode.BufferSize;
    WmiLogInfo->Wnode.HistoricalContext = Properties->Wnode.HistoricalContext;
    WmiLogInfo->Wnode.CountLost = Properties->Wnode.CountLost;

    if (Properties->Wnode.ClientContext)
        WmiLogInfo->Wnode.ClientContext = Properties->Wnode.ClientContext;
    else
        WmiLogInfo->Wnode.ClientContext = EVENT_TRACE_CLOCK_PERFCOUNTER;

    WmiLogInfo->Wnode.Flags = Properties->Wnode.Flags;
    WmiLogInfo->BufferSize = Properties->BufferSize;
    WmiLogInfo->MinimumBuffers = Properties->MinimumBuffers;
    WmiLogInfo->MaximumBuffers = Properties->MaximumBuffers;
    WmiLogInfo->MaximumFileSize = Properties->MaximumFileSize;
    WmiLogInfo->LogFileMode = Properties->LogFileMode;
    WmiLogInfo->FlushTimer = Properties->FlushTimer;
    WmiLogInfo->EnableFlags = Properties->EnableFlags;
    WmiLogInfo->AgeLimit = Properties->AgeLimit;
    WmiLogInfo->NumberOfBuffers = Properties->NumberOfBuffers;
    WmiLogInfo->FreeBuffers = Properties->FreeBuffers;
    WmiLogInfo->EventsLost = Properties->EventsLost;
    WmiLogInfo->BuffersWritten = Properties->BuffersWritten;
    WmiLogInfo->LogBuffersLost = Properties->LogBuffersLost;
    WmiLogInfo->RealTimeBuffersLost = Properties->RealTimeBuffersLost;
    WmiLogInfo->LoggerThreadId = Properties->LoggerThreadId;

    if (WmiLogInfo->Wnode.Flags & WNODE_FLAG_VERSIONED_PROPERTIES)
        WmiLogInfo->V2Options = 0;
    else
    WmiLogInfo->V2Options = Properties->V2Options;
}

#define DISABLE_VERSIONED_PROPERTIES \
    (~(WNODE_FLAG_ALL_DATA | WNODE_FLAG_VERSIONED_PROPERTIES))

static void WINAPI EtwpCopyInfoToProperties(PWMI_LOGGER_INFORMATION WmiLogInfo,
                                     PEVENT_TRACE_PROPERTIES_V2 Properties) {
    
    Properties->Wnode.BufferSize = WmiLogInfo->Wnode.BufferSize;
    Properties->Wnode.ProviderId = WmiLogInfo->RealTimeConsumerCount;
    Properties->Wnode.HistoricalContext = WmiLogInfo->Wnode.HistoricalContext;
    Properties->Wnode.CountLost = WmiLogInfo->Wnode.CountLost;
    Properties->Wnode.Guid = WmiLogInfo->Wnode.Guid;
    Properties->Wnode.ClientContext = WmiLogInfo->Wnode.ClientContext;
    Properties->Wnode.Flags = WmiLogInfo->Wnode.Flags;
    Properties->BufferSize = WmiLogInfo->BufferSize;
    Properties->MinimumBuffers = WmiLogInfo->MinimumBuffers;
    Properties->MaximumBuffers = WmiLogInfo->MaximumBuffers;
    Properties->MaximumFileSize = WmiLogInfo->MaximumFileSize;
    Properties->LogFileMode = WmiLogInfo->LogFileMode;
    Properties->FlushTimer = WmiLogInfo->FlushTimer;
    Properties->EnableFlags = WmiLogInfo->EnableFlags;
    Properties->AgeLimit = WmiLogInfo->AgeLimit;
    Properties->NumberOfBuffers = WmiLogInfo->NumberOfBuffers;
    Properties->FreeBuffers = WmiLogInfo->FreeBuffers;
    Properties->EventsLost = WmiLogInfo->EventsLost;
    Properties->BuffersWritten = WmiLogInfo->BuffersWritten;
    Properties->LogBuffersLost = WmiLogInfo->LogBuffersLost;
    Properties->RealTimeBuffersLost = WmiLogInfo->RealTimeBuffersLost;
    Properties->LoggerThreadId = WmiLogInfo->LoggerThreadId;

    if (Properties->Wnode.Flags & WNODE_FLAG_VERSIONED_PROPERTIES) {
        Properties->Wnode.Flags |= WNODE_FLAG_VERSIONED_PROPERTIES;
        Properties->V2Options = WmiLogInfo->V2Options;
    } else
        Properties->Wnode.Flags &= DISABLE_VERSIONED_PROPERTIES;
}

static ULONG WINAPI __XYZ_StartTraceW_V2(PTRACEHANDLE TraceHandle,
                                  PWSTR InstanceName,
                                  PEVENT_TRACE_PROPERTIES_V2 Properties) {
    
    ULONG FilterDescCount, ReturnedLength, LastError;
    ULONG InstanceNameLength = INFINITE, LogFileNameLength = INFINITE, BufferSize = 0;
    BOOLEAN IsLogFilePresent;
    PEVENT_FILTER_DESCRIPTOR FilterDesc = NULL;
    PWSTR LogFileName = NULL;

    if (!Properties || !TraceHandle) {
        RtlSetLastWin32Error(ERROR_INVALID_PARAMETER);
        return ERROR_INVALID_PARAMETER;
    }
    if (!InstanceName) {
        RtlSetLastWin32Error(ERROR_INVALID_NAME);
        return ERROR_INVALID_NAME;
    }

    LastError = EtwpValidateTraceProperties(Properties,
                                            &FilterDescCount,
                                            &FilterDesc,
                                            &ReturnedLength);
    if (LastError) {
        RtlSetLastWin32Error(LastError);
        return LastError;
    }

    if (_wcsicmp(InstanceName, L"NT Kernel Logger"))
        Properties->Wnode.Guid = SystemTraceControl;
    if (_wcsicmp(InstanceName, L"Circular Kernel Context Logger"))
        Properties->Wnode.Guid = CKCLGuid;

    do {
        ++InstanceNameLength;
    } while (InstanceName[InstanceNameLength]);
    BufferSize = ReturnedLength + (sizeof (wchar_t) * (InstanceNameLength + 1));

    if (Properties->LogFileNameOffset <= 0) {
        IsLogFilePresent = FALSE;
    } else {
        IsLogFilePresent = TRUE;
        LogFileName = (PWSTR)((PBYTE)Properties + Properties->LogFileNameOffset);

        do {
            ++LogFileNameLength;
        } while (LogFileName[LogFileNameLength]);

        BufferSize += (sizeof (wchar_t) * (LogFileNameLength + 1));
    }

    if (Properties->Wnode.BufferSize < BufferSize) {
        RtlSetLastWin32Error(ERROR_BAD_LENGTH);
        return ERROR_BAD_LENGTH;
    }

    if (Properties->LogFileMode & EVENT_TRACE_FILE_MODE_NEWFILE) {
        PWSTR pwc = wcschr(LogFileName, '%');
        if (!pwc || pwc != wcsrchr(LogFileName, '%') || !wcsstr(LogFileName, L"%d")) {
            RtlSetLastWin32Error(ERROR_INVALID_NAME);
            return ERROR_INVALID_NAME;
        }
    }

    // Start allocating buffer for NtTraceControl
    PWMI_LOGGER_INFORMATION WmiLogInfo = (PWMI_LOGGER_INFORMATION)calloc(Properties->Wnode.BufferSize, 1);

    EtwpCopyPropertiesToInfo(Properties, WmiLogInfo);
    WmiLogInfo->Wnode.Flags |= WNODE_FLAG_TRACED_GUID;
    RtlInitUnicodeString(&WmiLogInfo->LoggerName, InstanceName);
    if (IsLogFilePresent)
        RtlInitUnicodeString(&WmiLogInfo->LogFileName, LogFileName);


    NTSTATUS Status;
    Status = NtTraceControl(TraceControlStartLogger,
                            WmiLogInfo,
                            WmiLogInfo->Wnode.BufferSize,
                            WmiLogInfo,
                            WmiLogInfo->Wnode.BufferSize,
                            &ReturnedLength);

    LastError = RtlNtStatusToDosError(Status);
    if (!LastError) {
        ULONG Diff = 0;
        Properties->LoggerNameOffset = ReturnedLength;
        ULONG LoggerNameOffset = Properties->LoggerNameOffset;
        ULONG LogFileNameOffset = Properties->LogFileNameOffset;

        *TraceHandle = WmiLogInfo->Wnode.HistoricalContext;
        EtwpCopyInfoToProperties(WmiLogInfo, Properties);

        if (LoggerNameOffset > LogFileNameOffset)
            LogFileNameOffset = Properties->Wnode.BufferSize;
        Diff = LogFileNameOffset - LoggerNameOffset;

        if (sizeof (wchar_t) * (InstanceNameLength + 1) <= Diff)
            StringCbCopyW((PWSTR)((PBYTE)Properties + LoggerNameOffset), Diff, InstanceName);

        if (LogFileNameOffset > LoggerNameOffset)
            LoggerNameOffset = Properties->Wnode.BufferSize;
        Diff = LoggerNameOffset - LogFileNameOffset;

        if (WmiLogInfo->LogFileName.Length && WmiLogInfo->LogFileName.Length <= Diff)
            StringCbCopyW((PWSTR)((PBYTE)Properties + LogFileNameOffset), Diff, LogFileName);
    }

    // Cleanup
    free(WmiLogInfo);
    return LastError;
}

static ULONG WINAPI __XYZ_ControlTraceW_V2(TRACEHANDLE TraceHandle,
                                    PWSTR InstanceName,
                                    PEVENT_TRACE_PROPERTIES_V2 Properties,
                                    ULONG ControlCode) {
    
    ULONG FilterDescCount, ReturnedLength, LastError;
    ULONG InstanceNameLength = INFINITE, LogFileNameLength = INFINITE, BufferSize = 0;
    BOOLEAN IsLogFilePresent;
    PEVENT_FILTER_DESCRIPTOR FilterDesc = NULL;
    PWSTR LogFileName = NULL;

    if (!Properties) {
        RtlSetLastWin32Error(ERROR_INVALID_PARAMETER);
        return ERROR_INVALID_PARAMETER;
    }

    LastError = EtwpValidateTraceProperties(Properties,
                                            &FilterDescCount,
                                            &FilterDesc,
                                            &ReturnedLength);
    if (LastError) {
        RtlSetLastWin32Error(LastError);
        return LastError;
    }

    if (Properties->LogFileMode & EVENT_TRACE_FILE_MODE_APPEND &&
        ControlCode == EVENT_TRACE_CONTROL_UPDATE &&
        Properties->LogFileNameOffset > 0) {
        RtlSetLastWin32Error(ERROR_INVALID_PARAMETER);
        return ERROR_INVALID_PARAMETER;
    }

    if (InstanceName)
    {
        if (_wcsicmp(InstanceName, L"NT Kernel Logger"))
            Properties->Wnode.Guid = SystemTraceControl;
        if (_wcsicmp(InstanceName, L"Circular Kernel Context Logger"))
            Properties->Wnode.Guid = CKCLGuid;

        do {
            ++InstanceNameLength;
        } while (InstanceName[InstanceNameLength]);
        BufferSize = ReturnedLength + (sizeof (wchar_t) * (InstanceNameLength + 1));
    }
    else
        InstanceNameLength = 0;

    if (Properties->LogFileNameOffset <= 0) {
        IsLogFilePresent = FALSE;
    } else {
        IsLogFilePresent = TRUE;
        LogFileName = (PWSTR)((PBYTE)Properties + Properties->LogFileNameOffset);
        do {
            ++LogFileNameLength;
        } while (LogFileName[LogFileNameLength]);

        BufferSize += (sizeof (wchar_t) * (LogFileNameLength + 1));
    }

    // Start allocating buffer for NtTraceControl
    PWMI_LOGGER_INFORMATION WmiLogInfo = (PWMI_LOGGER_INFORMATION)calloc(Properties->Wnode.BufferSize, 1);

    EtwpCopyPropertiesToInfo(Properties, WmiLogInfo);
    WmiLogInfo->Wnode.Flags |= WNODE_FLAG_TRACED_GUID;
    WmiLogInfo->Wnode.HistoricalContext = TraceHandle;
    if (InstanceName)
        RtlInitUnicodeString(&WmiLogInfo->LoggerName, InstanceName);
    if (IsLogFilePresent)
        RtlInitUnicodeString(&WmiLogInfo->LogFileName, LogFileName);

    // Convert ControlCode to Function code for NtTraceControl
    TRACE_CONTROL_FUNCTION_CLASS FunctionCode;
    switch (ControlCode) {
        case EVENT_TRACE_CONTROL_QUERY:
            FunctionCode = TraceControlQueryLogger;
            break;

        case EVENT_TRACE_CONTROL_STOP:
            FunctionCode = TraceControlStopLogger;
            break;

        case EVENT_TRACE_CONTROL_UPDATE:
            FunctionCode = TraceControlUpdateLogger;
            break;

        case EVENT_TRACE_CONTROL_FLUSH:
            FunctionCode = TraceControlFlushLogger;
            break;

        case EVENT_TRACE_CONTROL_INCREMENT_FILE:
            FunctionCode = TraceControlIncrementLoggerFile;
            break;

        default:
            FunctionCode = (TRACE_CONTROL_FUNCTION_CLASS)INFINITE;
    }

    NTSTATUS Status;
    Status = NtTraceControl(FunctionCode,
                            WmiLogInfo,
                            WmiLogInfo->Wnode.BufferSize,
                            WmiLogInfo,
                            WmiLogInfo->Wnode.BufferSize,
                            &ReturnedLength);

    LastError = RtlNtStatusToDosError(Status);
    if (!LastError) {
        ULONG Diff = 0;
        // To-Be-Fixed:
        // Properties->LoggerNameOffset = ReturnedLength;
        ULONG LoggerNameOffset = Properties->LoggerNameOffset;
        ULONG LogFileNameOffset = Properties->LogFileNameOffset;

        EtwpCopyInfoToProperties(WmiLogInfo, Properties);

        if (LoggerNameOffset > LogFileNameOffset)
            LogFileNameOffset = Properties->Wnode.BufferSize;
        Diff = LogFileNameOffset - LoggerNameOffset;

        if (InstanceName && (sizeof (wchar_t) * (InstanceNameLength + 1) <= Diff) )
            StringCbCopyW((PWSTR)((PBYTE)Properties + LoggerNameOffset), Diff, InstanceName);

        if (LogFileNameOffset > LoggerNameOffset)
            LoggerNameOffset = Properties->Wnode.BufferSize;
        Diff = LoggerNameOffset - LogFileNameOffset;

        if (WmiLogInfo->LogFileName.Length && WmiLogInfo->LogFileName.Length <= Diff)
            StringCbCopyW((PWSTR)((PBYTE)Properties + LogFileNameOffset), Diff, LogFileName);
    }

    // Cleanup
    free(WmiLogInfo);
    return LastError;
}

// make function signature the same as documented API

ULONG WINAPI __XYZ_StartTraceW(
    PTRACEHANDLE TraceHandle,
    PWSTR InstanceName,
    PEVENT_TRACE_PROPERTIES Properties
) {
    return __XYZ_StartTraceW_V2(
        TraceHandle,
        InstanceName,
        (PEVENT_TRACE_PROPERTIES_V2)Properties
    );
}

ULONG WINAPI __XYZ_ControlTraceW(
    TRACEHANDLE TraceHandle,
    PWSTR InstanceName,
    PEVENT_TRACE_PROPERTIES Properties,
    ULONG ControlCode
) {
    return __XYZ_ControlTraceW_V2(
        TraceHandle,
        InstanceName,
        (PEVENT_TRACE_PROPERTIES_V2)Properties,
        ControlCode
    );
}

#endif // __TRACE_USE_UNDOCUMENTED__
