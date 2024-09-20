#include "babylon/anyflow/builtin/alias.h"

BABYLON_NAMESPACE_BEGIN
namespace anyflow {
namespace builtin {

int AliasProcessor::setup() noexcept {
  _source = vertex().anonymous_dependency(0);
  if (ABSL_PREDICT_FALSE(_source == nullptr)) {
    BABYLON_LOG(WARNING) << "depend num["
                         << vertex().anonymous_dependency_size()
                         << "] != 1 for " << vertex();
    return -1;
  }
  _target = vertex().anonymous_emit(0);
  if (ABSL_PREDICT_FALSE(_target == nullptr)) {
    BABYLON_LOG(WARNING) << "emit num[" << vertex().anonymous_emit_size()
                         << "] != 1 for " << vertex();
    return -1;
  }
  vertex().declare_trivial();
  return 0;
}

int AliasProcessor::on_activate() noexcept {
  if (_target->need_mutable()) {
    _source->declare_mutable();
  }
  return 0;
}

int AliasProcessor::process() noexcept {
  _target->forward(*_source);
  return 0;
}

void AliasProcessor::apply(GraphBuilder& builder, const ::std::string& alias,
                           const ::std::string& name) noexcept {
  auto& vertex = builder.add_vertex([] {
    return ::std::make_unique<AliasProcessor>();
  });
  vertex.set_name(
      std::string("AliasProcessor").append(std::to_string(++_s_idx)));
  vertex.anonymous_depend().to(name);
  vertex.anonymous_emit().to(alias);
}

::std::atomic<size_t> AliasProcessor::_s_idx {0};

} // namespace builtin
} // namespace anyflow
BABYLON_NAMESPACE_END
