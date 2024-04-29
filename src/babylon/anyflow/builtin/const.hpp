#pragma once

#include "babylon/anyflow/builtin/const.h"

BABYLON_NAMESPACE_BEGIN
namespace anyflow {
namespace builtin {

template <typename T>
void ConstProcessor::apply(GraphBuilder& builder, const ::std::string& data,
                           T&& value) noexcept {
  apply_without_value(builder, data).option(::std::forward<T>(value));
}

} // namespace builtin
} // namespace anyflow
BABYLON_NAMESPACE_END
