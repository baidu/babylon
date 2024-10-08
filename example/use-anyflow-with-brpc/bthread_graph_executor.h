#include "babylon/anyflow/closure.h"
#include "babylon/anyflow/executor.h"
#include "babylon/anyflow/vertex.h"

#include "bthread/bthread.h"

BABYLON_NAMESPACE_BEGIN
namespace anyflow {

class BthreadGraphExecutor : public GraphExecutor {
 public:
  static BthreadGraphExecutor& instance();
  virtual Closure create_closure() noexcept override;
  virtual int run(GraphVertex* vertex,
                  GraphVertexClosure&& closure) noexcept override;
  virtual int run(ClosureContext* closure,
                  Closure::Callback* callback) noexcept override;
};

} // namespace anyflow
BABYLON_NAMESPACE_END
