#include "babylon/anyflow/builtin/select.h"

BABYLON_NAMESPACE_BEGIN
namespace anyflow {
namespace builtin {

///////////////////////////////////////////////////////////////////////////////
// SelectProcessor begin
int SelectProcessor::setup() noexcept {
  if (vertex().anonymous_emit_size() != 1) {
    BABYLON_LOG(WARNING) << "emit num[" << vertex().anonymous_emit_size()
                         << "] != 1 for " << vertex();
    return -1;
  }
  vertex().declare_trivial();
  return 0;
}

int SelectProcessor::on_activate() noexcept {
  // 如果传递可变性，则按照0号输出的可变性，设置所有依赖的可变性
  if (vertex().anonymous_emit(0)->need_mutable()) {
    for (size_t i = 0; i < vertex().anonymous_dependency_size(); ++i) {
      vertex().anonymous_dependency(i)->declare_mutable();
    }
  }
  return 0;
}

int SelectProcessor::process() noexcept {
  for (size_t i = 0; i < vertex().anonymous_dependency_size(); ++i) {
    auto dependency = vertex().anonymous_dependency(i);
    if (dependency->ready()) {
      if (vertex().anonymous_emit(0)->forward(*dependency)) {
        return 0;
      } else {
        BABYLON_LOG(WARNING) << "forward dependency[" << i << "] failed";
        return -1;
      }
    }
  }
  BABYLON_LOG(WARNING) << "no dependency ready to forward";
  return -1;
}

void SelectProcessor::apply(GraphBuilder& builder, const ::std::string& dest,
                            const ::std::string& cond,
                            const ::std::string& true_src,
                            const ::std::string& false_src) noexcept {
  auto& vertex = builder.add_vertex([] {
    return ::std::make_unique<SelectProcessor>();
  });
  vertex.set_name(
      std::string("SelectProcessor").append(std::to_string(++_s_idx)));
  vertex.anonymous_depend().to(true_src).on(cond);
  vertex.anonymous_depend().to(false_src).unless(cond);
  vertex.anonymous_emit().to(dest);
}

::std::atomic<size_t> SelectProcessor::_s_idx {0};
// SelectProcessor end
///////////////////////////////////////////////////////////////////////////////

} // namespace builtin
} // namespace anyflow
BABYLON_NAMESPACE_END
