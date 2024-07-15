#pragma once

#include "babylon/anyflow/closure.h"
#include "babylon/anyflow/executor.h"
#include "babylon/future.h"
#include "babylon/logging/logger.h"

BABYLON_NAMESPACE_BEGIN
namespace anyflow {

template <typename S>
class ClosureContextImplement : public ClosureContext {
 public:
  inline ClosureContextImplement(GraphExecutor& executor) noexcept;
  virtual ~ClosureContextImplement() noexcept override;

 protected:
  virtual void wait_finish() noexcept override;
  virtual void notify_finish() noexcept override;
  virtual void wait_flush() noexcept override;
  virtual void notify_flush() noexcept override;

 private:
  Promise<void, S> _finished;
  Promise<void, S> _flushed;
};

///////////////////////////////////////////////////////////////////////////////
// Closure begin
Closure& Closure::operator=(Closure&& closure) {
  _context = std::move(closure._context);
  return *this;
}

Closure::Closure(Closure&& closure) noexcept
    : _context(::std::move(closure._context)) {}

Closure::Closure(ClosureContext* context) noexcept : _context(context) {}

bool Closure::finished() const noexcept {
  return _context->finished();
}

int32_t Closure::get() noexcept {
  return _context->get();
}

void Closure::wait() noexcept {
  return _context->wait();
}

int32_t Closure::error_code() const noexcept {
  return _context->error_code();
}

template <typename C>
void Closure::on_finish(C&& callback) noexcept {
  auto context = _context.release();
  context->on_finish(::std::move(callback));
}

template <typename S>
Closure Closure::create(GraphExecutor& executor) noexcept {
  return Closure(new ClosureContextImplement<S>(executor));
}
// Closure end
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// ClosureContext begin
ClosureContext* Closure::context() noexcept {
  return _context.get();
}

ClosureContext::ClosureContext(GraphExecutor& executor) noexcept {
  _executor = &executor;
}

void ClosureContext::finish(int32_t error_code) noexcept {
  Closure::Callback* callback = nullptr;
  if (mark_finished(error_code, callback)) {
    if (callback != nullptr && ABSL_PREDICT_FALSE(0 != invoke(callback))) {
      BABYLON_LOG(WARNING) << "closure[" << this << "] invoke callback["
                           << callback << "] failed delay to invoke on flush";
      _flush_callback = callback;
    }
  }
}

bool ClosureContext::mark_finished(int32_t error_code,
                                   Closure::Callback*& callback) noexcept {
  callback = _callback.load(::std::memory_order_relaxed);
  do {
    if (ABSL_PREDICT_FALSE(callback == SEALED_CALLBACK)) {
      return false;
    }
  } while (ABSL_PREDICT_FALSE(!_callback.compare_exchange_weak(
      callback, SEALED_CALLBACK, ::std::memory_order_acq_rel)));
  _error_code = error_code;
  notify_finish();
  return true;
}

void ClosureContext::run(Closure::Callback* callback) noexcept {
  (*callback)(Closure(this));
  delete callback;
}

template <typename C>
void ClosureContext::on_finish(C&& callback) noexcept {
  auto new_callback = new Closure::Callback(::std::move(callback));
  Closure::Callback* expected = nullptr;
  if (!_callback.compare_exchange_strong(expected, new_callback,
                                         ::std::memory_order_acq_rel)) {
    // 直接callback，有可能error_code还没生效
    // todo：这里有微概率阻塞一会儿，可以想想更好的同步方法
    wait_finish();
    run(new_callback);
  }
}

void ClosureContext::fire() noexcept {
  depend_data_sub();
  depend_vertex_sub();
}

bool ClosureContext::finished() const noexcept {
  return _callback.load(::std::memory_order_relaxed) == SEALED_CALLBACK;
}

void ClosureContext::depend_vertex_add() noexcept {
  _waiting_vertex_num.fetch_add(1, ::std::memory_order_acq_rel);
}

void ClosureContext::depend_vertex_sub() noexcept {
  int64_t waiting_num =
      _waiting_vertex_num.fetch_sub(1, ::std::memory_order_acq_rel) - 1;
  if (ABSL_PREDICT_FALSE(waiting_num == 0)) {
    Closure::Callback* callback = nullptr;
    if (mark_finished(-1, callback)) {
      log_unfinished_data();
      if (callback != nullptr && ABSL_PREDICT_FALSE(0 != invoke(callback))) {
        BABYLON_LOG(WARNING) << "closure[" << this << "] invoke callback["
                             << callback << "] failed delay to invoke on flush";
        _flush_callback = callback;
      }
    }

    if (_flush_callback != nullptr) {
      notify_flush();
      callback = _flush_callback;
      _flush_callback = nullptr;
      run(callback);
    } else {
      notify_flush();
    }
  }
}

void ClosureContext::depend_data_add() noexcept {
  _waiting_data_num.fetch_add(1, ::std::memory_order_acq_rel);
}

void ClosureContext::add_waiting_data(GraphData* data) noexcept {
  _waiting_data.emplace_back(data);
}

void ClosureContext::depend_data_sub() noexcept {
  int64_t waiting_num =
      _waiting_data_num.fetch_sub(1, ::std::memory_order_acq_rel) - 1;
  if (waiting_num == 0) {
    Closure::Callback* callback = nullptr;
    if (mark_finished(0, callback)) {
      if (callback != nullptr && ABSL_PREDICT_FALSE(0 != invoke(callback))) {
        _flush_callback = callback;
      }
    }
  }
}

void ClosureContext::all_data_num(size_t num) noexcept {
  _all_data_num = num;
}

int ClosureContext::get() noexcept {
  wait_finish();
  return _error_code;
}

int32_t ClosureContext::error_code() const noexcept {
  return _error_code;
}

void ClosureContext::wait() noexcept {
  wait_flush();
}
// ClosureContext end
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// ClosureContextImplement begin
template <typename S>
ClosureContextImplement<S>::ClosureContextImplement(
    GraphExecutor& executor) noexcept
    : ClosureContext(executor) {}

template <typename S>
ClosureContextImplement<S>::~ClosureContextImplement() noexcept {
  wait_flush();
}

template <typename S>
void ClosureContextImplement<S>::wait_finish() noexcept {
  _finished.get_future().get();
}

template <typename S>
void ClosureContextImplement<S>::notify_finish() noexcept {
  _finished.set_value();
}

template <typename S>
void ClosureContextImplement<S>::wait_flush() noexcept {
  _flushed.get_future().get();
}

template <typename S>
void ClosureContextImplement<S>::notify_flush() noexcept {
  _flushed.set_value();
}
// ClosureContextImplement end
///////////////////////////////////////////////////////////////////////////////

} // namespace anyflow
BABYLON_NAMESPACE_END
