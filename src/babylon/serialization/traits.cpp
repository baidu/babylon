#include "babylon/serialization/traits.h"

#if BABYLON_USE_PROTOBUF

// clang-format off
#include BABYLON_EXTERNAL(absl/strings/escaping.h) // absl::CEscape
// clang-format on

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
  ::std::string s = ::absl::CEscape({sv.data(), sv.size()});
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
