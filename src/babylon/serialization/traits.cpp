#include "babylon/serialization/traits.h"

#if BABYLON_USE_PROTOBUF

#if GOOGLE_PROTOBUF_VERSION >= 3000000 && \
    BABYLON_HAS_INCLUDE(<google / protobuf / stubs / strutil.h>)
#include "google/protobuf/stubs/strutil.h"
#else  // GOOGLE_PROTOBUF_VERSION < 3000000 ||
       // !BABYLON_HAS_INCLUDE(<google/protobuf/stubs/strutil.h>)
// 老版本protobuf，或者未输出strutil.h头文件，拷贝几个必要的函数出来
namespace google {
namespace protobuf {
// Calculates the length of the C-style escaped version of 'src'.
// Assumes that non-printable characters are escaped using octal sequences, and
// that UTF-8 bytes are not handled specially.
static inline size_t CEscapedLength(::babylon::StringView src) {
  static char c_escaped_len[256] = {
      4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 2, 4, 4, 2, 4, 4, // \t, \n, \r
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 1, 1, 2, 1, 1, 1,
      1, 2, 1, 1, 1, 1, 1, 1, 1, 1,                   // ", '
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // '0'..'9'
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 'A'..'O'
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, // 'P'..'Z', '\'
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 'a'..'o'
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 4, // 'p'..'z', DEL
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
  };

  size_t escaped_len = 0;
  for (decltype(src.size()) i = 0; i < src.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(src[i]);
    escaped_len += c_escaped_len[c];
  }
  return escaped_len;
}

// ----------------------------------------------------------------------
// Escapes 'src' using C-style escape sequences, and appends the escaped string
// to 'dest'. This version is faster than calling CEscapeInternal as it computes
// the required space using a lookup table, and also does not do any special
// handling for Hex or UTF-8 characters.
// ----------------------------------------------------------------------
static void CEscapeAndAppend(::babylon::StringView src, std::string* dest) {
  decltype(src.size()) escaped_len = CEscapedLength(src);
  if (escaped_len == src.size()) {
    dest->append(src.data(), src.size());
    return;
  }

  size_t cur_dest_len = dest->size();
  dest->resize(cur_dest_len + escaped_len);
  char* append_ptr = &(*dest)[cur_dest_len];

  for (decltype(src.size()) i = 0; i < src.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(src[i]);
    switch (c) {
      case '\n':
        *append_ptr++ = '\\';
        *append_ptr++ = 'n';
        break;
      case '\r':
        *append_ptr++ = '\\';
        *append_ptr++ = 'r';
        break;
      case '\t':
        *append_ptr++ = '\\';
        *append_ptr++ = 't';
        break;
      case '\"':
        *append_ptr++ = '\\';
        *append_ptr++ = '\"';
        break;
      case '\'':
        *append_ptr++ = '\\';
        *append_ptr++ = '\'';
        break;
      case '\\':
        *append_ptr++ = '\\';
        *append_ptr++ = '\\';
        break;
      default:
        if (!isprint(c)) {
          *append_ptr++ = '\\';
          *append_ptr++ = '0' + c / 64;
          *append_ptr++ = '0' + (c % 64) / 8;
          *append_ptr++ = '0' + c % 8;
        } else {
          *append_ptr++ = c;
        }
        break;
    }
  }
}
} // namespace protobuf
} // namespace google
#endif // GOOGLE_PROTOBUF_VERSION < 3000000 ||
       // !BABYLON_HAS_INCLUDE(<google/protobuf/stubs/strutil.h>)

BABYLON_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
// Serialization begin
Serialization::Serializer::~Serializer() noexcept {}

Serialization::Serializer* Serialization::serializer_for_name(
    StringView class_full_name) noexcept {
  auto& serializer_map = serializers();
  auto iter = serializer_map.find(class_full_name);
  if (ABSL_PREDICT_FALSE(iter == serializer_map.end())) {
    return nullptr;
  }
  return iter->second.get();
}

::absl::flat_hash_map<::std::string,
                      ::std::unique_ptr<Serialization::Serializer>,
                      ::std::hash<StringView>, ::std::equal_to<StringView>>&
Serialization::serializers() noexcept {
  static ::absl::flat_hash_map<::std::string, ::std::unique_ptr<Serializer>,
                               ::std::hash<StringView>,
                               ::std::equal_to<StringView>>
      instance;
  return instance;
}
// Serialization end
////////////////////////////////////////////////////////////////////////////////

Serialization::PrintStream::~PrintStream() noexcept {
  flush();
}

void Serialization::PrintStream::set_stream(ZeroCopyOutputStream& os) noexcept {
  _os = &os;
}

Serialization::ZeroCopyOutputStream&
Serialization::PrintStream::stream() noexcept {
  return *_os;
}

void Serialization::PrintStream::set_indent_level(size_t level) noexcept {
  _indent_level = level;
}

size_t Serialization::PrintStream::indent_level() const noexcept {
  return _indent_level;
}

bool Serialization::PrintStream::indent() noexcept {
  ++_indent_level;
  return true;
}

bool Serialization::PrintStream::outdent() noexcept {
  --_indent_level;
  return true;
}

bool Serialization::PrintStream::print_raw(const char* data,
                                           size_t size) noexcept {
  if (_at_start_of_line) {
    _at_start_of_line = false;
    if (ABSL_PREDICT_FALSE(!print_blank(_indent_level * 2))) {
      return false;
    }
  }

  while (size > _buffer_size) {
    if (_buffer_size > 0) {
      ::memcpy(_buffer, data, _buffer_size);
      data += _buffer_size;
      size -= _buffer_size;
    }
    void* buffer;
    int buffer_size;
    auto success = _os->Next(&buffer, &buffer_size);
    if (ABSL_PREDICT_FALSE(!success)) {
      _buffer = nullptr;
      _buffer_size = 0;
      return false;
    }
    _buffer = static_cast<char*>(buffer);
    _buffer_size = buffer_size;
  }
  ::memcpy(_buffer, data, size);
  _buffer += size;
  _buffer_size -= size;
  return true;
}

bool Serialization::PrintStream::print_raw(StringView sv) noexcept {
  return print_raw(sv.data(), sv.size());
}

bool Serialization::PrintStream::print_string(StringView sv) noexcept {
  ::std::string s;
  ::google::protobuf::CEscapeAndAppend({sv.data(), sv.size()}, &s);
  return print_raw("\"") && print_raw(s) && print_raw("\"");
}

bool Serialization::PrintStream::start_new_line() noexcept {
  auto success = print_raw("\n", 1);
  _at_start_of_line = true;
  return success;
}

void Serialization::PrintStream::flush() noexcept {
  if (_buffer_size > 0) {
    _os->BackUp(_buffer_size);
    _buffer_size = 0;
  }
}

bool Serialization::PrintStream::print_blank(size_t size) noexcept {
  while (size > _buffer_size) {
    if (_buffer_size > 0) {
      ::memset(_buffer, ' ', _buffer_size);
      size -= _buffer_size;
    }
    void* buffer;
    int buffer_size;
    auto success = _os->Next(&buffer, &buffer_size);
    if (ABSL_PREDICT_FALSE(!success)) {
      _buffer = nullptr;
      _buffer_size = 0;
      return false;
    }
    _buffer = static_cast<char*>(buffer);
    _buffer_size = buffer_size;
  }
  ::memset(_buffer, ' ', size);
  _buffer += size;
  _buffer_size -= size;
  return true;
}

bool SerializationHelper::consume_unknown_field(uint32_t tag,
                                                CodedInputStream& is) noexcept {
  uint64_t varint;
  switch (tag & 0x7) {
    case WireFormatLite::WIRETYPE_VARINT:
      if (ABSL_PREDICT_FALSE(!is.ReadVarint64(&varint))) {
        return false;
      }
      break;
    case WireFormatLite::WIRETYPE_FIXED32:
      if (ABSL_PREDICT_FALSE(!is.Skip(4))) {
        return false;
      }
      break;
    case WireFormatLite::WIRETYPE_FIXED64:
      if (ABSL_PREDICT_FALSE(!is.Skip(8))) {
        return false;
      }
      break;
    case WireFormatLite::WIRETYPE_LENGTH_DELIMITED:
      if (ABSL_PREDICT_FALSE(!is.ReadVarint64(&varint))) {
        return false;
      }
      if (ABSL_PREDICT_FALSE(!is.Skip(varint))) {
        return false;
      }
      break;
    default:
      return false;
  }
  return true;
}

BABYLON_NAMESPACE_END

#endif // BABYLON_USE_PROTOBUF
