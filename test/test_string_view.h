#pragma once

#include <babylon/string_view.h>

using ::babylon::StringView;

size_t get_std_string_view_size(::std::string_view sv);
size_t get_babylon_string_view_size(StringView sv);
size_t get_std_string_view_ref_size(const ::std::string_view& sv);
size_t get_babylon_string_view_ref_size(const StringView& sv);
