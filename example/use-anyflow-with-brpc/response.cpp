#include "babylon/anyflow/vertex.h"
#include "babylon/application_context.h"

#include "search.pb.h"

class Response : public ::babylon::anyflow::GraphProcessor {
  virtual int process() noexcept override {
    auto committer = response.emit();
    // Fill result to service response
    for (auto& one_result : *result) {
      auto one_response_result = committer->add_result();
      one_response_result->set_doc_id(::std::get<0>(one_result));
      one_response_result->set_score(::std::get<1>(one_result));
    }
    return 0;
  }

  // clang-format off
  ANYFLOW_INTERFACE(
    ANYFLOW_DEPEND_DATA((::std::vector<::std::tuple<uint64_t, float>>), result)
    ANYFLOW_EMIT_DATA(SearchResponse, response)
  )
  // clang-format on
};

BABYLON_REGISTER_COMPONENT(Response, "Response",
                           ::babylon::anyflow::GraphProcessor);
