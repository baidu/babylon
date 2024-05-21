#include "babylon/anyflow/builder.h"

#include <iostream>

using ::babylon::anyflow::GraphBuilder;
using ::babylon::anyflow::GraphProcessor;

struct AddProcessor : public GraphProcessor {
  virtual int process() noexcept override {
    *c.emit() = *a + *b;
    return 0;
  }

  ANYFLOW_INTERFACE(ANYFLOW_DEPEND_DATA(int32_t, a, 0)
                        ANYFLOW_DEPEND_DATA(int32_t, b, 1)
                            ANYFLOW_EMIT_DATA(int32_t, c))
};

struct SubtractProcessor : public GraphProcessor {
  virtual int process() noexcept override {
    *c.emit() = *a - *b;
    return 0;
  }

  ANYFLOW_INTERFACE(ANYFLOW_DEPEND_DATA(int32_t, a, 0)
                        ANYFLOW_DEPEND_DATA(int32_t, b, 1)
                            ANYFLOW_EMIT_DATA(int32_t, c))
};

struct MultiplyProcessor : public GraphProcessor {
  virtual int process() noexcept override {
    *c.emit() = (*a) * (*b);
    return 0;
  }

  ANYFLOW_INTERFACE(ANYFLOW_DEPEND_DATA(int32_t, a, 0)
                        ANYFLOW_DEPEND_DATA(int32_t, b, 1)
                            ANYFLOW_EMIT_DATA(int32_t, c))
};

int main() {
  // let A = 10, B = 5
  // try to prove that (A + B) * (A - B) = A * A - B * B
  int input_a = 10;
  int input_b = 5;
  int res_left = 0;
  int res_right = 0;
  {
    GraphBuilder builder;

    auto& v1 = builder.add_vertex([] {
      return ::std::unique_ptr<AddProcessor>(new AddProcessor);
    });
    v1.named_depend("a").to("A");
    v1.named_depend("b").to("B");
    v1.named_emit("c").to("AddRes");

    auto& v2 = builder.add_vertex([] {
      return ::std::unique_ptr<SubtractProcessor>(new SubtractProcessor);
    });
    v2.named_depend("a").to("A");
    v2.named_depend("b").to("B");
    v2.named_emit("c").to("SubtractRes");

    auto& v3 = builder.add_vertex([] {
      return ::std::unique_ptr<MultiplyProcessor>(new MultiplyProcessor);
    });
    v3.named_depend("a").to("AddRes");
    v3.named_depend("b").to("SubtractRes");
    v3.named_emit("c").to("FinalRes");

    builder.finish();
    auto graph = builder.build();
    auto a = graph->find_data("A");
    auto b = graph->find_data("B");
    auto final_res = graph->find_data("FinalRes");

    *(a->emit<int>()) = input_a;
    *(b->emit<int>()) = input_b;
    graph->run(final_res);
    res_left = *final_res->value<int>();
  }
  {
    GraphBuilder builder;

    auto& v1 = builder.add_vertex([] {
      return ::std::unique_ptr<MultiplyProcessor>(new MultiplyProcessor);
    });
    v1.named_depend("a").to("A");
    v1.named_depend("b").to("A");
    v1.named_emit("c").to("MultiplyResForA");

    auto& v2 = builder.add_vertex([] {
      return ::std::unique_ptr<MultiplyProcessor>(new MultiplyProcessor);
    });
    v2.named_depend("a").to("B");
    v2.named_depend("b").to("B");
    v2.named_emit("c").to("MultiplyResForB");

    auto& v3 = builder.add_vertex([] {
      return ::std::unique_ptr<SubtractProcessor>(new SubtractProcessor);
    });
    v3.named_depend("a").to("MultiplyResForA");
    v3.named_depend("b").to("MultiplyResForB");
    v3.named_emit("c").to("FinalRes");

    builder.finish();
    auto graph = builder.build();
    auto a = graph->find_data("A");
    auto b = graph->find_data("B");
    auto final_res = graph->find_data("FinalRes");

    *(a->emit<int>()) = input_a;
    *(b->emit<int>()) = input_b;
    graph->run(final_res);
    res_right = *final_res->value<int>();
  }
  ::std::cout << "(A + B) * (A - B) = " << res_left << '\n';
  ::std::cout << "A * A - B * B = " << res_right << '\n';
  return 0;
}
