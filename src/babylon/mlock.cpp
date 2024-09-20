#include "babylon/mlock.h"

#include "babylon/logging/logger.h" // BABYLON_LOG

// clang-format off
#include BABYLON_EXTERNAL(absl/strings/str_split.h) // absl::StrSplit
// clang-format on

#include <sys/mman.h> // ::mlock

#include <fstream> // std::ifstream

BABYLON_NAMESPACE_BEGIN

MemoryLocker::~MemoryLocker() noexcept {
  stop();
}

MemoryLocker& MemoryLocker::set_check_interval(
    std::chrono::seconds interval) noexcept {
  _check_interval = interval;
  return *this;
}

MemoryLocker& MemoryLocker::set_filter(const Filter& filter) noexcept {
  _filter = filter;
  return *this;
}

int MemoryLocker::start() noexcept {
  if (_thread.joinable()) {
    return -1;
  }
  _stoped = ::std::promise<void>();
  _thread = ::std::thread([&] {
    auto stoped = _stoped.get_future();
    do {
      check_and_lock();
      _round++;
    } while (stoped.wait_for(_check_interval) == ::std::future_status::timeout);
  });
  return 0;
}

int MemoryLocker::stop() noexcept {
  if (_thread.joinable()) {
    _stoped.set_value();
    _thread.join();
    unlock_regions();
  }
  return 0;
}

size_t MemoryLocker::round() const noexcept {
  return _round;
}

size_t MemoryLocker::locked_bytes() const noexcept {
  return _locked_bytes;
}

int MemoryLocker::last_errno() const noexcept {
  return _last_errno;
}

MemoryLocker& MemoryLocker::instance() noexcept {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wexit-time-destructors"
  static MemoryLocker object;
#pragma GCC diagnostic pop
  return object;
}

// private:
bool MemoryLocker::filter_none(StringView) noexcept {
  return false;
}

void MemoryLocker::check_and_lock() noexcept {
  ::std::ifstream ifs("/proc/self/maps");
  ::std::string line;
  ::std::string zero = "0";
  ::std::vector<::absl::string_view> fields;
  ::std::unordered_map<const void*, size_t> regions;
  while (::std::getline(ifs, line)) {
    fields = ::absl::StrSplit(line, ' ', ::absl::SkipEmpty());
    // 跳过无法解析的行
    if (fields.size() < 6
        // 跳过没有对应inode的区域
        || fields[4] == zero
        // 跳过不可读的区域
        || fields[1].find('r') != 0
        // 跳过非私有映射
        || fields[1].find('p') != 3
        // 跳过被过滤的文件
        || _filter(StringView {fields[5].data(), fields[5].size()})) {
      continue;
    }

    // 解析区域起止地址
    auto* begin = line.c_str();
    char* end = nullptr;
    auto region_start = strtoull(begin, &end, 16);
    if (*end != '-') {
      continue;
    }
    begin = end + 1;
    auto region_end = strtoull(begin, &end, 16);
    if (*end != ' ') {
      continue;
    }

    // 区域记入待锁定集合
    regions.emplace(reinterpret_cast<const void*>(region_start),
                    region_end - region_start);
  }

  lock_regions(::std::move(regions));
}

void MemoryLocker::lock_regions(
    ::std::unordered_map<const void*, size_t>&& regions) noexcept {
  // 无变化，不做处理
  if (_locked_regions == regions) {
    return;
  }

  // 低频操作且mlock可支持多次调用
  // 就不精细求变化差集了，解锁再重新加锁
  unlock_regions();

  // 锁定区域并记录锁定量和可能的错误
  int last_errno = 0;
  size_t locked_bytes = 0;
  for (auto& pair : regions) {
    if (0 == ::mlock(pair.first, pair.second)) {
      locked_bytes += pair.second;
    } else {
      last_errno = errno;
    }
  }
  _locked_regions = ::std::move(regions);
  _last_errno = last_errno;
  _locked_bytes = locked_bytes;
  BABYLON_LOG(INFO) << "mlock " << _locked_regions.size()
                    << " regions with bytes " << _locked_bytes << " errno "
                    << _last_errno;
}

void MemoryLocker::unlock_regions() noexcept {
  for (auto& pair : _locked_regions) {
    ::munlock(pair.first, pair.second);
  }
  BABYLON_LOG(INFO) << "munlock " << _locked_regions.size() << " regions";
  _locked_regions.clear();
  _last_errno = 0;
  _locked_bytes = 0;
}

BABYLON_NAMESPACE_END
