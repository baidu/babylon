#include "babylon/anyflow/vertex.h"
#include "babylon/application_context.h"

#include "yaml-cpp/yaml.h"

#include <random>

class Recall : public ::babylon::anyflow::GraphProcessor {
 public:
  int initialize(const ::babylon::Any& option) {
    // Get threshold from option
    auto option_node = option.get<::YAML::Node>();
    _threshold = (*option_node)["threshold"].as<float>();
    return 0;
  }

  virtual int process() noexcept override {
    auto committer = result.emit();
    auto& result_vector = *committer;
    // Do some real recall instead this random generate
    for (size_t i = 1; i < 10; ++i) {
      float score = _gen() % 1000 / 1000.0;
      if (score > _threshold) {
        result_vector.push_back(i);
      }
    }
    return 0;
  }

  ::std::mt19937 _gen {::std::random_device {}()};
  float _threshold {0};

  // clang-format off
  ANYFLOW_INTERFACE(
    ANYFLOW_DEPEND_DATA(::std::string, query)
    ANYFLOW_EMIT_DATA(::std::vector<uint64_t>, result)
  )
  // clang-format on
};

BABYLON_REGISTER_COMPONENT(Recall, "Recall",
                           ::babylon::anyflow::GraphProcessor);
