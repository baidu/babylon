#include "babylon/anyflow/vertex.h"
#include "babylon/application_context.h"

#include "search.pb.h"

class UserProfile : public ::babylon::anyflow::GraphProcessor {
  virtual int process() noexcept override {
    // Do some real profile fetching work instead.
    profile = ::std::to_string(*id);
    return 0;
  }

  // clang-format off
  ANYFLOW_INTERFACE(
    ANYFLOW_DEPEND_DATA(uint64_t, id)
    ANYFLOW_EMIT_DATA(::std::string, profile)
  )
  // clang-format on
};

BABYLON_REGISTER_COMPONENT(UserProfile, "UserProfile",
                           ::babylon::anyflow::GraphProcessor);
