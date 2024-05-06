#pragma once

#include "babylon/anyflow/vertex.h"

BABYLON_NAMESPACE_BEGIN
namespace anyflow {
namespace builtin {

// 常量算子
class ConstProcessor : public GraphProcessor {
 public:
  virtual int setup() noexcept override;
  virtual int process() noexcept override;

  template <typename T>
  inline static void apply(GraphBuilder& builder, const ::std::string& data,
                           T&& value) noexcept;

 private:
  static GraphVertexBuilder& apply_without_value(
      GraphBuilder& builder, const ::std::string& data) noexcept;

  static std::atomic<size_t> _s_idx;
};

} // namespace builtin
} // namespace anyflow
BABYLON_NAMESPACE_END

#include "babylon/anyflow/builtin/const.hpp"
