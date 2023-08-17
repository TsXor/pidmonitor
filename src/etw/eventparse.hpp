#include <cstdint>
#include <cstring>
#include <bit>
#include <functional>
#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>
#include <winerror.h>
#include <minwindef.h>
#include <minwinbase.h>
#include "tdh_patched.h"
#include "string_dict_key.hpp"

/*
    Thanks to:
        TraceEvent https://github.com/Biswa96/TraceEvent
        CppReference https://en.cppreference.com
    Hatred to:
        Microsoft
*/

#define PROFILER_CONTEXT_PREALLOC_SIZE 8192
#define PROFILER_CONTEXT_REALLOC_UNIT  1024

#define INT_MOD_CEIL(x, mod) (x % mod ? x - x % mod + mod : x) // compiler will optimize it
#define PTR_AT_OFFSET_AS_TYPE(ptr, offset, type) (*(type* )(((unsigned char* )(ptr)) + offset))
#define PTR_MARCH_IN_BYTE(ptr, offset) ((void* )(((unsigned char* )(ptr)) + offset))

#define INHERIT_STD_ERROR(newcls, inherited) \
class newcls : public inherited { \
public: \
    explicit newcls(const std::string& __arg) : inherited(__arg) {}; \
    explicit newcls(const char* __arg) : inherited(__arg) {}; \
    newcls(const newcls&) = default; \
    newcls& operator=(const newcls&) = default; \
    newcls(newcls&&) = default; \
    newcls& operator=(newcls&&) = default; \
    virtual ~newcls() noexcept = default; \
};


static const size_t guid_cstr_length = sizeof(L"{AAAABBBB-CCCC-DDDD-EEEE-FFFFGGGGHHHH}") / sizeof(wchar_t);

// Specialize std::hash
template<>
struct std::hash<GUID> {
    size_t operator()(const GUID& guid) const noexcept {
        const std::uint64_t* p = reinterpret_cast<const std::uint64_t*>(&guid);
        std::hash<std::uint64_t> hash;
        return hash(p[0]) ^ hash(p[1]);
    }
};

// The compare operator is required by std::unordered_map
inline bool operator==(const GUID& a, const GUID& b) {
    return std::memcmp(&a, &b, sizeof(GUID)) == 0;
}


namespace EtwEventParse {

// **********
// Declarations
// **********

typedef enum {
    prop_is_array      = 1 << 0,
    prop_fixed_size    = 1 << 1
} MY_PROPERTY_FLAGS;

// fun fact: Microsoft uses USHORT (aka uint16_t) to represent
//           lengths in ETW, so using a larger type in your
//           code makes no sense. You can also consider a user
//           data blob to be smaller than 64KB.
typedef struct {
    union {
        struct _non_struct_info {
            USHORT in_type;
            USHORT out_type;
        } non_struct_info;
        struct _struct_info {
            USHORT start_idx;
            USHORT span_n;
        } struct_info;
    };
    uint8_t ms_flags;
    uint8_t my_flags;
    uint16_t _unused;
    union {
        USHORT size;
        USHORT size_ref_idx;
    };
    union {
        USHORT count;
        USHORT count_ref_idx;
    };
} anyprop_t;

INHERIT_STD_ERROR(ParsingError, std::runtime_error);
INHERIT_STD_ERROR(TdhError, std::runtime_error);


class Parser;
class ProfilerContext;
class ClassProfile;
class ParsedUserData;

using prop_lst_t = std::vector<anyprop_t>;
using prop_map_t = std::unordered_map<wstring_view_with_source, USHORT>;
using offset_lst_t = std::vector<uint32_t>;
using event_name2id_map_t = std::unordered_map<wstring_view_with_source, USHORT>;
using event_id2profile_map_t = std::unordered_map<USHORT, ClassProfile*>;

void profile_userdata(EVENT_RECORD* event_record, ProfilerContext* res,
                      ClassProfile* profile, Parser* parser);


class ProfilerContext {
    friend void profile_userdata(EVENT_RECORD* event_record, ProfilerContext* res,
                                 ClassProfile* profile, Parser* parser);
private:
    TRACE_EVENT_INFO* event_info_ptr;
    ULONG event_info_size;
public:
    ProfilerContext() {
        this->event_info_ptr = (TRACE_EVENT_INFO *)malloc(PROFILER_CONTEXT_PREALLOC_SIZE);
        this->event_info_size = PROFILER_CONTEXT_PREALLOC_SIZE;
    }
    void adjust_buffer(size_t expected) {
        this->event_info_size = INT_MOD_CEIL(expected, PROFILER_CONTEXT_REALLOC_UNIT);
        this->event_info_ptr = (TRACE_EVENT_INFO* )realloc(this->event_info_ptr, this->event_info_size);
    }
    ~ProfilerContext() {
        free(this->event_info_ptr);
    }
};

class ClassProfile {
    friend void profile_userdata(EVENT_RECORD* event_record, ProfilerContext* res,
                                 ClassProfile* profile, Parser* parser);
    friend class ParsedUserData;
    friend class Parser;
private:
    // minimal data required
    prop_lst_t prop_lst;
    prop_map_t prop_map;
    // calculated to increase speed
    bool optimized;
    std::vector<USHORT> offset_table;
    std::vector<USHORT> dyn_count_table;
    std::vector<USHORT> dyn_idx_table;
    
    void optimize(void) {
        if (this->optimized) return;
        // space -> time
        USHORT offset = 0, dyn_count = 0;
        for (USHORT i = 0; i < this->prop_lst.size(); i++) {
            this->offset_table.emplace_back(offset);
            this->dyn_count_table.emplace_back(dyn_count);
            anyprop_t& prop_info = prop_lst[i];
            if ((prop_info.my_flags & prop_fixed_size) && !(prop_info.ms_flags & PropertyParamCount)) {
                offset += prop_info.size * prop_info.count;
            } else {
                dyn_count++;
                this->dyn_idx_table.emplace_back(i);
            }
        }
        this->optimized = true;
    }
    ClassProfile() {
        this->optimized = false;
    }
public:
    ~ClassProfile() {}
};

inline USHORT __get_internal_size_prop(void* datptr, uint32_t offset, USHORT in_type) {
    auto target_ptr = PTR_MARCH_IN_BYTE(datptr, offset);
    USHORT ret;
    switch (in_type) {
        case TDH_INTYPE_INT8:
            ret = *(int8_t* )target_ptr; break;
        case TDH_INTYPE_UINT8:
            ret = *(uint8_t* )target_ptr; break;
        case TDH_INTYPE_INT16:
            ret = *(int16_t* )target_ptr; break;
        case TDH_INTYPE_UINT16:
            ret = *(uint16_t* )target_ptr; break;
        case TDH_INTYPE_INT32:
            ret = *(int32_t* )target_ptr; break;
        case TDH_INTYPE_UINT32:
        case TDH_INTYPE_HEXINT32:
            ret = *(uint32_t* )target_ptr; break;
        case TDH_INTYPE_INT64:
            ret = *(int64_t* )target_ptr; break;
        case TDH_INTYPE_UINT64:
        case TDH_INTYPE_HEXINT64:
            ret = *(uint64_t* )target_ptr; break;
        
        default: throw std::runtime_error("prop indicating size/count is not valid");
    }
    return ret;
}

inline size_t __calculate_sid_size(void* ptr) {
    BYTE sub_count = PTR_AT_OFFSET_AS_TYPE(ptr, offsetof(SID, SubAuthorityCount), BYTE);
    return offsetof(SID, SubAuthority) + sub_count * sizeof(DWORD);
}

inline USHORT __cauculate_prop_size_manually(void* datptr, uint32_t offset, USHORT in_type, USHORT* result_ptr) {
    auto target_ptr = PTR_MARCH_IN_BYTE(datptr, offset);
    switch (in_type) {
        case TDH_INTYPE_UNICODESTRING:
            // null-terminated utf16 string
            if (result_ptr)
                *result_ptr = (USHORT)( sizeof(wchar_t) *
                    (wcslen((wchar_t* )target_ptr) + 1) );
            return 0;
        case TDH_INTYPE_ANSISTRING:
            // null-terminated ansi string
            if (result_ptr)
                *result_ptr = (USHORT)( sizeof(char) *
                    (strlen((char* )target_ptr) + 1) );
            return 0;
        case TDH_INTYPE_COUNTEDSTRING:
        case TDH_INTYPE_MANIFEST_COUNTEDSTRING:
            // utf16 string with its first 2 bytes as length in small-endian
            if (result_ptr)
                *result_ptr = (USHORT)( sizeof(uint16_t) +
                    (*(uint16_t* )target_ptr) * sizeof(wchar_t) );
            return 0;
        case TDH_INTYPE_COUNTEDANSISTRING:
        case TDH_INTYPE_MANIFEST_COUNTEDANSISTRING:
            // ansi string with its first 2 bytes as length in small-endian
            if (result_ptr)
                *result_ptr = (USHORT)( sizeof(uint16_t) +
                    (*(uint16_t* )target_ptr) * sizeof(char) );
            return 0;
        case TDH_INTYPE_REVERSEDCOUNTEDSTRING:
            // utf16 string with its first 2 bytes as length in big-endian
            if (result_ptr)
                *result_ptr = (USHORT)( sizeof(uint16_t) +
                    std::byteswap(*(uint16_t* )target_ptr) * sizeof(wchar_t) );
            return 0;
        case TDH_INTYPE_REVERSEDCOUNTEDANSISTRING:
            // ansi string with its first 2 bytes as length in big-endian
            if (result_ptr)
                *result_ptr = (USHORT)( sizeof(uint16_t) +
                    std::byteswap(*(uint16_t* )target_ptr) * sizeof(char) );
            return 0;
        case TDH_INTYPE_MANIFEST_COUNTEDBINARY:
            // binary with its first 2 bytes as length in small-endian
            if (result_ptr)
                *result_ptr = (USHORT)( sizeof(uint16_t) +
                    (*(uint16_t* )target_ptr) );
            return 0;
        case TDH_INTYPE_SID:
        case TDH_INTYPE_WBEMSID:
            if (result_ptr)
                *result_ptr = (USHORT)__calculate_sid_size(target_ptr);
            return 0;
        case TDH_INTYPE_NONNULLTERMINATEDSTRING:
        case TDH_INTYPE_NONNULLTERMINATEDANSISTRING:
        case TDH_INTYPE_BINARY:
        case TDH_INTYPE_HEXDUMP:
            // For these types, its impossible to retrieve its size
            // if PropertyParamLength is not set and given size is 0
            // so if it is not the last one, we cannot handle.
            // Return a mark value to let upper functions handle.
            return 1;
        default:
            // For other types, their size are fixed.
            // Return a mark value to let upper functions handle.
            return 2;
    }
}

typedef struct {
    USHORT size;
    USHORT count;
} dyn_prop_info_t;

class ParsedUserData {
private:
    // minimal data required
    ClassProfile* profile_ptr;
    void* data;
    // calculated to increase speed
    std::vector<dyn_prop_info_t> dyn_info_table;
    std::vector<USHORT> dyn_offset_table;
public:
    ParsedUserData(ClassProfile* prof, void* dat) {
        this->profile_ptr = prof;
        this->data = dat;
    }

    USHORT name2index(const std::wstring_view& prop_name) {
        return this->profile_ptr->prop_map[prop_name];
    }

    USHORT name2index(const std::wstring& prop_name) {
        return this->profile_ptr->prop_map[std::wstring_view(prop_name)];
    }

    USHORT get_prop_offset(USHORT index) {
        USHORT dyn_needed_count = this->profile_ptr->dyn_count_table[index];
        for (USHORT i = this->dyn_offset_table.size(); i < dyn_needed_count; i++) {
            USHORT prop_real_size, prop_real_count;
            USHORT dyn_prop_idx = this->profile_ptr->dyn_idx_table[i];
            anyprop_t& dyn_prop = this->profile_ptr->prop_lst[dyn_prop_idx];

            if (dyn_prop.my_flags & prop_fixed_size) {
                prop_real_size = dyn_prop.size;
            } else if (dyn_prop.ms_flags & PropertyParamLength) {
                anyprop_t& size_prop = this->profile_ptr->prop_lst[dyn_prop.size_ref_idx];
                prop_real_size = __get_internal_size_prop(
                    this->data, this->get_prop_offset(dyn_prop.size_ref_idx),
                    size_prop.non_struct_info.in_type);
            } else {
                USHORT code = __cauculate_prop_size_manually(
                    this->data, this->get_prop_offset(dyn_prop_idx),
                    dyn_prop.non_struct_info.in_type, &prop_real_size);
                assert(code == 0);
            }

            if (dyn_prop.ms_flags & PropertyParamCount) {
                anyprop_t& count_prop = this->profile_ptr->prop_lst[dyn_prop.count_ref_idx];
                prop_real_count = __get_internal_size_prop(
                    this->data, this->get_prop_offset(dyn_prop.count_ref_idx),
                    count_prop.non_struct_info.in_type);
            } else {
                prop_real_count = dyn_prop.count;
            }

            this->dyn_info_table.emplace_back(prop_real_size, prop_real_count);
            this->dyn_offset_table.emplace_back(prop_real_size * prop_real_count
                + (i == 0) ? 0 : this->dyn_offset_table[i - 1] );
        }
        return this->profile_ptr->offset_table[index] + this->dyn_offset_table[dyn_needed_count];
    }

    anyprop_t& get_prop_info(USHORT index) {
        return this->profile_ptr->prop_lst[index];
    }

    // warning: do not use this function on a property that does not have dynamic size
    dyn_prop_info_t& get_dyn_prop_info(USHORT index) {
        return this->dyn_info_table[this->profile_ptr->dyn_count_table[index] + 1];
    }
};

void profile_userdata(EVENT_RECORD* event_record, ProfilerContext* res,
                      ClassProfile* profile, Parser* parser) {
    
    ULONG getevt_used_size;
    ULONG getevt_result = TdhGetEventInformation(
        event_record, 0, NULL, res->event_info_ptr, &getevt_used_size);
    if (getevt_result == ERROR_INSUFFICIENT_BUFFER) {
        res->adjust_buffer(getevt_used_size);
        getevt_result = TdhGetEventInformation(
            event_record, 0, NULL, res->event_info_ptr, &getevt_used_size);
    }
    if (getevt_result != ERROR_SUCCESS) throw TdhError("TdhGetEventInformation failed");

    if (res->event_info_ptr->TopLevelPropertyCount <= 0) return;

    // insert event_name->event_id mapping
    auto cls_name_wcstr = (wchar_t* )PTR_MARCH_IN_BYTE(
        res->event_info_ptr, res->event_info_ptr->TaskNameOffset);
    std::wstring cls_name_wstr = cls_name_wcstr;
    parser->event_name2id_map.emplace(std::piecewise_construct,
        std::forward_as_tuple(std::move(cls_name_wstr)),
        std::forward_as_tuple(event_record->EventHeader.EventDescriptor.Id)
    );
    
    void* userdata_ptr = event_record->UserData;
    USHORT userdata_len = event_record->UserDataLength;
    
    // length of userdata is USHORT, and property count must be smaller than that
    // so USHORT is enough for property count
    for (USHORT i = 0; i < res->event_info_ptr->TopLevelPropertyCount; i++) {

        // https://learn.microsoft.com/zh-cn/windows/win32/etw/using-tdhformatproperty-to-consume-event-data

        auto prop_info = res->event_info_ptr->EventPropertyInfoArray[i];
        auto prop_name_wcstr = prop_info.NameOffset
            ? (wchar_t* )PTR_MARCH_IN_BYTE(res->event_info_ptr, prop_info.NameOffset)
            : L"";
        uint8_t my_flags = 0;

        USHORT prop_size, prop_real_size, prop_size_type, prop_count, prop_real_count;
        bool is_fixed_size, is_array, is_struct;

        // One thing to notice: observaton with a modified version of TraceEvent
        // on PsProvGuid has shown that most props are basic data types (basic C
        // number types and string), and they DO NOT HAVE ANY FLAG.

        // if a prop have PropertyStruct, it is a struct "indicator"*
        // *Note: It seems that a property that have PropertyStruct flag
        //        is only an indicator that does not really exist in the
        //        event blob.
        is_struct = prop_info.Flags & PropertyStruct;

        // decide prop size
        if (prop_info.Flags & PropertyParamLength) {
            // size of this property is stored in another property
            prop_size = prop_info.lengthPropertyIndex;
            is_fixed_size = false;
        } else {
            // special case for incorrectly-defined IPV6 addresses
            if ( !is_struct
                 && (prop_info.nonStructType.InType == TDH_INTYPE_BINARY)
                 && (prop_info.nonStructType.OutType == TDH_OUTTYPE_IPV6) ) {
                prop_size = 16;
                is_fixed_size = true;
                my_flags |= prop_fixed_size;
                prop_count = 1;
                is_array = false;
                is_struct = false;
                goto push_prop;
            }
            prop_size = prop_info.length;
            if ((prop_info.Flags & PropertyParamFixedLength) || is_struct) {
                // Microsoft never say if a struct indicator really
                // have zero size, so size of a struct indicator is
                // always considered as fixed.
                is_fixed_size = true;
            } else {
                prop_size_type = __cauculate_prop_size_manually(
                    userdata_ptr, 0, prop_info.nonStructType.InType, NULL);
                is_fixed_size = prop_size_type == 2;
            }

            if (!is_fixed_size && prop_size_type == 1
                && (i + 1) != res->event_info_ptr->TopLevelPropertyCount) {
                // see comments in __cauculate_prop_size_manually
                throw ParsingError("unknown sized prop not at tail");
            }
        }
        if (is_fixed_size) my_flags |= prop_fixed_size;

        // decide prop count
        if (prop_info.Flags & PropertyParamCount) {
            // count of this property is stored in another property
            prop_count = prop_info.countPropertyIndex;
            is_array = true;
        } else {
            // > Note that PropertyParamFixedCount is a new flag and is ignored
            // > by many decoders. Without the PropertyParamFixedCount flag,
            // > decoders will assume that a property is an array if it has
            // > either a count parameter or a fixed count other than 1. The
            // > PropertyParamFixedCount flag allows for fixed-count arrays with
            // > one element to be propertly decoded as arrays.
            prop_count = prop_info.count;
            is_array = (prop_info.Flags & PropertyParamFixedCount) || (prop_count != 1);
        }
        if (is_array) my_flags |= prop_is_array;

        push_prop:
            profile->prop_lst.emplace_back(
                is_struct ? prop_info.structType.StructStartIndex : prop_info.nonStructType.InType,
                is_struct ? prop_info.structType.NumOfStructMembers : prop_info.nonStructType.OutType,
                (uint8_t)prop_info.Flags, (uint8_t)my_flags,
                /*_unused:*/(uint16_t)0,
                prop_size, prop_count
            );
            std::wstring prop_name_wstr = prop_name_wcstr;
            profile->prop_map.emplace(std::piecewise_construct,
                std::forward_as_tuple(std::move(prop_name_wstr)),
                std::forward_as_tuple(i)
            );
    }

    // We are making some wrong assumptions and debugging awaits!
    //if (prop_offset != userdata_len) throw ParsingError("calculated size does not match real size");
}



class Parser {
    friend void profile_userdata(EVENT_RECORD* event_record, ProfilerContext* res,
                                 ClassProfile* profile, Parser* parser);
private:
    GUID _guid;
    ProfilerContext _ctx;
    event_name2id_map_t event_name2id_map;
    event_id2profile_map_t event_id2profile_map;
public:
    Parser(GUID guid) {
        this->_guid = guid;
    }

    void load(std::wstring_view path) {
        // TODO
    }

    void dump(std::wstring_view path) {
        // TODO
    }

    ParsedUserData parse_userdata(EVENT_RECORD* event_record) {
        ClassProfile* profile;
        auto event_id = event_record->EventHeader.EventDescriptor.Id;
        auto find_result_it = this->event_id2profile_map.find(event_id);
        if (find_result_it == this->event_id2profile_map.end()) {
            // key does not exist
            profile = new ClassProfile();
            profile_userdata(event_record, &(this->_ctx), profile, this);
            this->event_id2profile_map[event_id] = profile;
        } else {
            profile = find_result_it->second;
        }
        return ParsedUserData(profile, event_record->UserData);
    }

    USHORT name2id(const std::wstring_view& prop_name) {
        return this->event_name2id_map[prop_name];
    }

    USHORT name2id(const std::wstring& prop_name) {
        return this->event_name2id_map[std::wstring_view(prop_name)];
    }
};

} // namespace EtwEventParse
