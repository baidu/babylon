#include "babylon/anyflow/vertex.h"
#include "babylon/application_context.h"

#include "search.pb.h"

class Parse : public ::babylon::anyflow::GraphProcessor {
  virtual int process() noexcept override {
    user_id = request->user_id();
    // Use ref to make it zero copy
    query.emit().ref(request->query());
    return 0;
  }

  // clang-format off
  ANYFLOW_INTERFACE(
    ANYFLOW_DEPEND_DATA(SearchRequest, request)
    ANYFLOW_EMIT_DATA(uint64_t, user_id)
    ANYFLOW_EMIT_DATA(::std::string, query)
  )
  // clang-format on
};

BABYLON_REGISTER_COMPONENT(Parse, "Parse", ::babylon::anyflow::GraphProcessor);
