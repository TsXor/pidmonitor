#pragma once
#ifndef __STRING_DICT_KEY_HPP__
#define __STRING_DICT_KEY_HPP__

#include <string>
#include <utility>

// only used as dict key
// when initialized with lvalue, it is normal view
// when initialized with rvalue, it moves value inside
// its hash value is not related to source
template <typename CharT>
class basic_string_view_with_source {
    using string_t = std::basic_string<CharT>;
    using strview_t = std::basic_string_view<CharT>;
private:
    string_t* source = nullptr;
    strview_t view;
public:
    basic_string_view_with_source(const string_t& str):
        view(str) {}
    basic_string_view_with_source(const strview_t& str):
        view(str) {}
    basic_string_view_with_source(string_t&& str) {
        this->source = new string_t(str);
        this->view = strview_t(*(this->source));
    }
    const strview_t& get_raw_view(void) const {
        return this->view;
    }
    ~basic_string_view_with_source() {
        if (this->source) delete this->source;
    }
};

using string_view_with_source = basic_string_view_with_source<char>;
using wstring_view_with_source = basic_string_view_with_source<wchar_t>;

// Specialize std::hash
template<>
struct std::hash<string_view_with_source> {
    size_t operator()(const string_view_with_source& svws) const noexcept {
        std::hash<std::string_view> hasher;
        return hasher(svws.get_raw_view());
    }
};

template<>
struct std::hash<wstring_view_with_source> {
    size_t operator()(const wstring_view_with_source& svws) const noexcept {
        std::hash<std::wstring_view> hasher;
        return hasher(svws.get_raw_view());
    }
};

// The compare operator is required by std::unordered_map
inline bool operator==(const string_view_with_source& a, const string_view_with_source& b) {
    return a.get_raw_view() == b.get_raw_view();
}

inline bool operator==(const wstring_view_with_source& a, const wstring_view_with_source& b) {
    return a.get_raw_view() == b.get_raw_view();
}

#endif
