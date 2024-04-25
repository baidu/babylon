#pragma once

#include "babylon/anyflow/vertex.h"

BABYLON_NAMESPACE_BEGIN
namespace anyflow {
namespace builtin {

// 选择算子
// 从多个依赖中选择第一个满足条件的输出
class SelectProcessor : public GraphProcessor {
public:
    virtual int setup() noexcept override;
    virtual int on_activate() noexcept override;
    virtual int process() noexcept override;

    // 最常见的dest = cond ? true_src : false_src简写，默认option
    static void apply(GraphBuilder& builder,
                      const ::std::string& dest,
                      const ::std::string& cond,
                      const ::std::string& true_src,
                      const ::std::string& false_src) noexcept;

private:
    static ::std::atomic<size_t> _s_idx;
};

} // builtin
} // anyflow
BABYLON_NAMESPACE_END
