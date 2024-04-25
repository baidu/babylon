#include "babylon/concurrent/counter.h"

BABYLON_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
// ConcurrentAdder begin
ssize_t ConcurrentAdder::value() const noexcept {
    ssize_t sum = 0;
    _storage.for_each([&] (const ssize_t& value) {
        sum += value;
    });
    return sum;
}

void ConcurrentAdder::reset() noexcept {
    _storage.for_each([&] (ssize_t& value) {
        value = 0;
    });
}
// ConcurrentAdder end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ConcurrentMaxer begin
ssize_t ConcurrentMaxer::value() const noexcept {
    ssize_t max_value = 0;
    value(max_value);
    return max_value;
}

bool ConcurrentMaxer::value(ssize_t& max_value) const noexcept {
    bool has_result = false;
    ssize_t result = ::std::numeric_limits<ssize_t>::min();
    _storage.for_each([&] (const Slot& slot) {
        if (slot.version == _version) {
            if (slot.value > result) {
                result = slot.value;
                has_result = true;
            }
        }
    });
    if (has_result) {
        max_value = result;
    }
    return has_result;
}

void ConcurrentMaxer::reset() noexcept {
    ++_version;
}
// ConcurrentMaxer end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ConcurrentSummer begin
ConcurrentSummer::Summary ConcurrentSummer::value() const noexcept {
#ifdef __x86_64__
    __m128i summary_value = _mm_setzero_si128();
#else
    int64x2_t summary_value = vmovq_n_s64(0);
#endif
    _storage.for_each([&] (const Summary& value) {
        // 等效语句summary += value
        // 但涉及128bit原子写操作，需要特殊处理
#ifdef __x86_64__
        __m128i local_value = _mm_load_si128(
                reinterpret_cast<const __m128i*>(&value));
        summary_value = _mm_add_epi64(summary_value, local_value);
#else
        int64x2_t local_value = vld1q_s64(
                reinterpret_cast<const int64_t*>(&value));
        summary_value = vaddq_s64(summary_value, local_value);
#endif
    });
    return {summary_value[0], static_cast<size_t>(summary_value[1])};
}
// ConcurrentSummer end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END
