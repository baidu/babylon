#include "babylon/anyflow/builtin/const.h"

BABYLON_NAMESPACE_BEGIN
namespace anyflow {
namespace builtin {

int ConstProcessor::setup() noexcept {
  if (vertex().anonymous_emit_size() != 1) {
    BABYLON_LOG(WARNING) << "emit num[" << vertex().anonymous_emit_size()
                         << "] != 1 for " << vertex();
    return -1;
  }

  vertex().declare_trivial();
  return 0;
}

int ConstProcessor::process() noexcept {
  auto value = option<::babylon::Any>();
  vertex().anonymous_emit(0)->emit<::babylon::Any>()->ref(*value);
  return 0;
}

GraphVertexBuilder& ConstProcessor::apply_without_value(
    GraphBuilder& builder, const ::std::string& data) noexcept {
  auto& vertex = builder.add_vertex([] {
    return ::std::make_unique<ConstProcessor>();
  });
  vertex.set_name(
      std::string("ConstProcessor").append(std::to_string(++_s_idx)));
  vertex.anonymous_emit().to(data);
  return vertex;
}

::std::atomic<size_t> ConstProcessor::_s_idx {0};

} // namespace builtin
} // namespace anyflow
BABYLON_NAMESPACE_END
