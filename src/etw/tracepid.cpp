#include "trace.hpp"
#include "string_dict_key.hpp"
#include <unordered_map>
#include <stdexcept>
#include "unicode/encoding.hpp"
#define __TRACEPID_SELF_INCLUSION__
#include "../../include/tracepid.hpp"
#undef __TRACEPID_SELF_INCLUSION__
#include "../../include/name2pid.hpp"


#define PTR_AT_OFFSET_AS_TYPE(ptr, offset, type) (*(type* )(((unsigned char* )(ptr)) + offset))
#define PTR_MARCH_IN_BYTE(ptr, offset) ((void* )(((unsigned char* )(ptr)) + offset))


static const auto empty_pid_set = pid_set_t();

static inline std::wstring_view __path_get_last(std::wstring_view full_path) {
    size_t last_sep_idx = full_path.length() - 1;
    while (full_path[last_sep_idx] != L'\\') {
        if (last_sep_idx == 0) return std::wstring(full_path);
        last_sep_idx--;
    }
    last_sep_idx++;
    return full_path.substr(last_sep_idx);
}


class PidStoragePerImage::Data {
public:
    std::unordered_map<wstring_view_with_source, pid_set_t> image_map;
    std::unordered_multimap<string_view_with_source, std::wstring_view> short_name_map;
    Data() {};
    ~Data() {};
};

PidStoragePerImage::PidStoragePerImage() { this->_data = new Data(); }
PidStoragePerImage::~PidStoragePerImage() { delete this->_data; }

void PidStoragePerImage::add_pid(const std::wstring_view name_view, win_pid_t pid) {
    auto image_map_target_pair = this->_data->image_map.find(wstring_view_with_source(name_view));
    if (image_map_target_pair == this->_data->image_map.end()) return; // this image is not traced
    image_map_target_pair->second.emplace(pid);
}

void PidStoragePerImage::del_pid(const std::string_view short_name_view, win_pid_t pid) {
    auto short_image_map_target_pair = this->_data->short_name_map.find(string_view_with_source(short_name_view));
    //if (short_image_map_target_pair == this->_short_name_map.end()) return; // this image is not traced
    for (; short_image_map_target_pair != this->_data->short_name_map.end(); short_image_map_target_pair++) {
        auto n_erased = this->_data->image_map[wstring_view_with_source(short_image_map_target_pair->second)].erase(pid);
        if (n_erased) break;
    }
}

void PidStoragePerImage::add_image_name(const std::wstring_view name_view, pid_set_t&& init) {
    auto name_persist = std::wstring(name_view);
    this->_data->image_map.emplace(std::piecewise_construct,
        std::forward_as_tuple(std::move(name_persist)),
        std::forward_as_tuple(std::move(init)) // empty set
    );

    auto name_short_persist = sWtoA(name_persist);
    if (name_short_persist.length() > 14) name_short_persist.resize(14);
    this->_data->short_name_map.emplace(std::piecewise_construct,
        std::forward_as_tuple(std::move(name_short_persist)),
        std::forward_as_tuple(name_persist.data(), name_persist.length())
    );
}

void PidStoragePerImage::add_image_name(const std::wstring_view name_view) {
    this->add_image_name(name_view, NameToPidWmi(name_view));
}

void PidStoragePerImage::del_image_name(const std::wstring_view name_view) {
    this->_data->image_map.erase(wstring_view_with_source(name_view));
    
    auto name_short = sWtoA(name_view);
    if (name_short.length() > 14) name_short.resize(14);
    auto name_short_target_pair = this->_data->short_name_map.find(string_view_with_source(name_short));
    for (; name_short_target_pair != this->_data->short_name_map.end(); name_short_target_pair++) {
        if (name_short_target_pair->second == name_view) {
            this->_data->short_name_map.erase(name_short_target_pair);
            break;
        }
    }
}

std::vector<std::wstring_view> PidStoragePerImage::get_all_image_names(void) {
    std::vector<std::wstring_view> ret;
    ret.resize(this->_data->image_map.size());
    for (auto image_map_kvp : this->_data->image_map) {
        ret.emplace_back(image_map_kvp.first.get_raw_view());
    }
    return ret;
}

const pid_set_t& PidStoragePerImage::get_image_pids(const std::wstring_view name_view) {
    auto image_map_target_pair = this->_data->image_map.find(wstring_view_with_source(name_view));
    if (image_map_target_pair == this->_data->image_map.end()) throw std::invalid_argument("image is not traced");
    return image_map_target_pair->second;
}


class PidStorageAll::Data {
public:
    std::unordered_set<wstring_view_with_source> image_name_set;
    std::unordered_set<string_view_with_source> short_name_set;
    pid_set_t pid_set;
    Data() {};
    ~Data() {};
};

PidStorageAll::PidStorageAll() { this->_data = new Data(); }
PidStorageAll::~PidStorageAll() { delete this->_data; }

void PidStorageAll::add_pid(const std::wstring_view name_view, win_pid_t pid) {
    size_t n_found = this->_data->image_name_set.count(wstring_view_with_source(name_view));
    if (!n_found) return; // this image is not traced
    this->_data->pid_set.emplace(pid);
}

void PidStorageAll::del_pid(const std::string_view short_name_view, win_pid_t pid) {
    size_t n_found = this->_data->short_name_set.count(string_view_with_source(short_name_view));
    if (!n_found) return; // this image is not traced
    this->_data->pid_set.erase(pid);
}

void PidStorageAll::add_image_name(const std::wstring_view name_view, pid_set_t&& init) {
    auto name_persist = std::wstring(name_view);
    this->_data->image_name_set.emplace(std::move(name_persist));

    this->_data->pid_set.merge(std::move(init));

    auto name_short_persist = sWtoA(name_persist);
    if (name_short_persist.length() > 14) name_short_persist.resize(14);
    this->_data->short_name_set.emplace(std::move(name_short_persist));
}

void PidStorageAll::add_image_name(const std::wstring_view name_view) {
    this->add_image_name(name_view, NameToPidWmi(name_view));
}

void PidStorageAll::del_image_name(const std::wstring_view name_view) {
    this->_data->image_name_set.erase(wstring_view_with_source(name_view));

    for (auto pid_of_name : NameToPidWmi(name_view)) {
        this->_data->pid_set.erase(pid_of_name);
    }
    
    auto name_short = sWtoA(name_view);
    if (name_short.length() > 14) name_short.resize(14);
    this->_data->short_name_set.erase(string_view_with_source(name_short));
}

std::vector<std::wstring_view> PidStorageAll::get_all_image_names(void) {
    std::vector<std::wstring_view> ret;
    ret.resize(this->_data->image_name_set.size());
    for (auto image_name : this->_data->image_name_set) {
        ret.emplace_back(image_name.get_raw_view());
    }
    return ret;
}

const pid_set_t& PidStorageAll::get_all_pids(void) {
    return this->_data->pid_set;
}


class PidTracer::Data {
public:
    EtwTrace::Session etw_session;
    EtwTrace::Tracer etw_tracer;
    std::vector<PidStorageBase*> pids_ptrs;
    Data(const std::wstring_view name):
        etw_session(name, PsProvGuid),
        etw_tracer(true, name, __tracepid_etw_handler_cb, this) {}
};

PidTracer::PidTracer(const std::wstring_view name) { this->_data = new Data(name); }
PidTracer::~PidTracer() { delete this->_data; }

void PidStorageBase::subscribe(PidTracer& tracer) {tracer._data->pids_ptrs.emplace_back(this);}

static VOID WINAPI __tracepid_etw_handler_cb(PEVENT_RECORD EventRecord) {
    win_pid_t pid; auto tracer_data_ptr = (PidTracer::Data* )EventRecord->UserContext;
    
    switch (EventRecord->EventHeader.EventDescriptor.Id) {
        case 1: { // 创建进程
            pid = PTR_AT_OFFSET_AS_TYPE(EventRecord->UserData, 0, uint32_t);
            std::wstring_view image_name = __path_get_last(std::wstring_view((wchar_t* )PTR_MARCH_IN_BYTE(EventRecord->UserData, 60)));
            for (auto pids_ptr : tracer_data_ptr->pids_ptrs) {
                try {
                    pids_ptr->acquire_lock();
                    pids_ptr->add_pid(image_name, pid);
                    pids_ptr->release_lock();
                } catch(...) {
                    return;
                }
            }
        } break;
        
        case 2: { // 进程退出
            pid = EventRecord->EventHeader.ProcessId;
            std::string_view short_image_name = std::string_view((char* )PTR_MARCH_IN_BYTE(EventRecord->UserData, 84));
            for (auto pids_ptr : tracer_data_ptr->pids_ptrs) {
                try {
                    pids_ptr->acquire_lock();
                    pids_ptr->del_pid(short_image_name, pid);
                    pids_ptr->release_lock();
                } catch(...) {
                    return;
                }
            }
        } break;
        
        default: break;
    }
}
