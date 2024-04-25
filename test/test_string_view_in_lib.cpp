#include "test_string_view.h"

size_t get_std_string_view_size(::std::string_view sv) {
    return sv.size();
}

size_t get_babylon_string_view_size(StringView sv) {
    return sv.size();
}

size_t get_std_string_view_ref_size(const ::std::string_view& sv) {
    return sv.size();
}

size_t get_babylon_string_view_ref_size(const StringView& sv) {
    return sv.size();
}
