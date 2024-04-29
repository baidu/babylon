#pragma once

#include "babylon/anyflow/vertex.h"

BABYLON_NAMESPACE_BEGIN
namespace anyflow {
namespace builtin {

// 重命名算子
class AliasProcessor : public GraphProcessor {
 public:
  virtual int setup() noexcept override;
  virtual int on_activate() noexcept override;
  virtual int process() noexcept override;

  static void apply(GraphBuilder&, const ::std::string& alias,
                    const ::std::string& data) noexcept;

 private:
  static ::std::atomic<size_t> _s_idx;

  GraphDependency* _source {nullptr};
  GraphData* _target {nullptr};
};

} // namespace builtin
} // namespace anyflow
BABYLON_NAMESPACE_END
