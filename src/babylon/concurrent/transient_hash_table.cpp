#include "babylon/concurrent/transient_hash_table.h"

BABYLON_NAMESPACE_BEGIN

namespace internal {
namespace concurrent_transient_hash_table {

////////////////////////////////////////////////////////////////////////////////
// Group begin
alignas(BABYLON_CACHELINE_SIZE)
::std::atomic<int8_t> Group::s_dummy_controls[] = {
    {DUMMY_CONTROL}, {DUMMY_CONTROL}, {DUMMY_CONTROL}, {DUMMY_CONTROL},
    {DUMMY_CONTROL}, {DUMMY_CONTROL}, {DUMMY_CONTROL}, {DUMMY_CONTROL},
    {DUMMY_CONTROL}, {DUMMY_CONTROL}, {DUMMY_CONTROL}, {DUMMY_CONTROL},
    {DUMMY_CONTROL}, {DUMMY_CONTROL}, {DUMMY_CONTROL}, {DUMMY_CONTROL},
    {DUMMY_CONTROL}, {DUMMY_CONTROL}, {DUMMY_CONTROL}, {DUMMY_CONTROL},
    {DUMMY_CONTROL}, {DUMMY_CONTROL}, {DUMMY_CONTROL}, {DUMMY_CONTROL},
    {DUMMY_CONTROL}, {DUMMY_CONTROL}, {DUMMY_CONTROL}, {DUMMY_CONTROL},
    {DUMMY_CONTROL}, {DUMMY_CONTROL}, {DUMMY_CONTROL}, {DUMMY_CONTROL},
};
// Group end
////////////////////////////////////////////////////////////////////////////////

// 当库本身用-std=c++14编译时，强制产出的符号为常量符号
// 会和使用者采用-std=c++17编译时产生的唯一符号发生重定义冲突
// 这里对-std=c++14的情况改写为弱符号
#if __cplusplus < 201703L
ABSL_ATTRIBUTE_WEAK constexpr size_t Group::SIZE;
ABSL_ATTRIBUTE_WEAK constexpr size_t Group::GROUP_MASK;
ABSL_ATTRIBUTE_WEAK constexpr size_t Group::GROUP_MASK_BITS;
ABSL_ATTRIBUTE_WEAK constexpr size_t Group::CHECKER_MASK;
ABSL_ATTRIBUTE_WEAK constexpr size_t Group::CHECKER_MASK_BITS;
ABSL_ATTRIBUTE_WEAK constexpr int8_t Group::DUMMY_CONTROL;
ABSL_ATTRIBUTE_WEAK constexpr int8_t Group::BUSY_CONTROL;
ABSL_ATTRIBUTE_WEAK constexpr int8_t Group::EMPTY_CONTROL;
#endif // __cplusplus < 201703L

ABSL_ATTRIBUTE_WEAK uintptr_t constexpr_symbol_generator() {
    return reinterpret_cast<uintptr_t>(&Group::SIZE)
        + reinterpret_cast<uintptr_t>(&Group::GROUP_MASK)
        + reinterpret_cast<uintptr_t>(&Group::GROUP_MASK_BITS)
        + reinterpret_cast<uintptr_t>(&Group::CHECKER_MASK)
        + reinterpret_cast<uintptr_t>(&Group::CHECKER_MASK_BITS)
        + reinterpret_cast<uintptr_t>(&Group::DUMMY_CONTROL)
        + reinterpret_cast<uintptr_t>(&Group::BUSY_CONTROL)
        + reinterpret_cast<uintptr_t>(&Group::EMPTY_CONTROL);
}

} // concurrent_transient_hash_table
} // internal

BABYLON_NAMESPACE_END
