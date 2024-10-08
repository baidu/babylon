#include "babylon/anyflow/vertex.h"
#include "babylon/application_context.h"

#include "yaml-cpp/yaml.h"

#include <random>

class Rank : public ::babylon::anyflow::GraphProcessor {
 public:
  int initialize(const ::babylon::Any& option) {
    // Get limit from option
    auto option_node = option.get<::YAML::Node>();
    _limit = (*option_node)["limit"].as<size_t>();
    return 0;
  }

  virtual int process() noexcept override {
    auto committer = result.emit();
    // Merge variadic depend
    for (size_t i = 0; i < vertex().anonymous_dependency_size(); ++i) {
      auto one_result =
          vertex().anonymous_dependency(i)->value<::std::vector<uint64_t>>();
      for (auto id : *one_result) {
        committer->push_back({id, 0});
      }
    }
    ::std::sort(committer->begin(), committer->end());
    auto new_end = ::std::unique(committer->begin(), committer->end());
    committer->resize(new_end - committer->begin());

    // Do real ranking instead of this random score assign
    for (auto& one_result : *committer) {
      ::std::get<1>(one_result) = _gen() % 500 / 1000.0 + 0.25;
    }
    ::std::sort(committer->begin(), committer->end(),
                [](const ::std::tuple<uint64_t, float>& left,
                   const ::std::tuple<uint64_t, float>& right) {
                  return ::std::get<1>(left) > ::std::get<1>(right);
                });
    return 0;
  }

  ::std::mt19937 _gen {::std::random_device {}()};
  size_t _limit {0};

  // clang-format off
  ANYFLOW_INTERFACE(
    ANYFLOW_EMIT_DATA((::std::vector<::std::tuple<uint64_t, float>>), result)
  )
  // clang-format on
};

BABYLON_REGISTER_COMPONENT(Rank, "Rank", ::babylon::anyflow::GraphProcessor);
