#pragma once

#include "babylon/concurrent/counter.h"

#include "bvar/bvar.h"

BABYLON_NAMESPACE_BEGIN

// ::bvar::Adder的替换实现
class BvarAdder : public ::bvar::Variable {
 public:
  // ::bvar::Window要求的必要定义
  using value_type = ssize_t;
  using Op = ::bvar::detail::AddTo<value_type>;
  using InvOp = ::bvar::detail::MinusFrom<value_type>;
  using sampler_type =
      ::bvar::detail::ReducerSampler<BvarAdder, value_type, Op, InvOp>;

  // ::bvar::Variable的序列展示功能需要的周期收集器
  class SeriesSampler : public ::bvar::detail::Sampler {
   public:
    SeriesSampler(BvarAdder* owner, const Op& op) noexcept;
    virtual void take_sample() noexcept override;
    void describe(::std::ostream& os) noexcept;

   private:
    BvarAdder* _owner;
    ::bvar::detail::Series<value_type, Op> _series;
  };

  BvarAdder() noexcept = default;
  BvarAdder(BvarAdder&&) = delete;
  BvarAdder(const BvarAdder&) = delete;
  BvarAdder& operator=(BvarAdder&&) = delete;
  BvarAdder& operator=(const BvarAdder&) = delete;
  ~BvarAdder() noexcept;

  BvarAdder(const ::butil::StringPiece& name) noexcept;
  BvarAdder(const ::butil::StringPiece& prefix,
            const ::butil::StringPiece& name) noexcept;

  // 计数接口
  inline BvarAdder& operator<<(value_type value) noexcept;

  // ::bvar::Window要求的获取采样器接口
  sampler_type* get_sampler() noexcept;

  // ::bvar::detail::ReducerSampler要求的汇聚接口
  // 具备逆运算时实现get_value，否则实现reset
  value_type get_value() const noexcept;
  value_type reset() noexcept;

  // ::bvar::detail::ReducerSampler要求的正反操作器
  Op op() const noexcept;
  InvOp inv_op() const noexcept;

 private:
  // ::bvar::Variable要求的单值展示接口
  virtual void describe(::std::ostream& os, bool) const noexcept override;

  // ::bvar::Variable要求的序列展示接口
  virtual int describe_series(
      ::std::ostream& os,
      const ::bvar::SeriesOptions& options) const noexcept override;

  // ::bvar::Variable在expose时需要启动序列采样器
  virtual int expose_impl(
      const ::butil::StringPiece& prefix, const ::butil::StringPiece& name,
      ::bvar::DisplayFilter display_filter) noexcept override;

  ::babylon::ConcurrentAdder _adder;
  sampler_type* _sampler {nullptr};
  SeriesSampler* _series_sampler {nullptr};
};

// ::bvar::Maxer的替换实现
class BvarMaxer : public ::bvar::Variable {
 public:
  // ::bvar::Window要求的必要定义
  using value_type = ssize_t;
  using Op = ::bvar::detail::MaxTo<value_type>;
  using InvOp = ::bvar::detail::VoidOp;
  using sampler_type =
      ::bvar::detail::ReducerSampler<BvarMaxer, value_type, Op, InvOp>;

  BvarMaxer() noexcept = default;
  BvarMaxer(BvarMaxer&&) = delete;
  BvarMaxer(const BvarMaxer&) = delete;
  BvarMaxer& operator=(BvarMaxer&&) = delete;
  BvarMaxer& operator=(const BvarMaxer&) = delete;
  ~BvarMaxer() noexcept;

  // 计数接口
  inline BvarMaxer& operator<<(value_type value) noexcept;

  // ::bvar::Window要求的获取采样器接口
  sampler_type* get_sampler() noexcept;

  // ::bvar::detail::ReducerSampler要求的汇聚接口
  // 具备逆运算时实现get_value，否则实现reset
  value_type get_value() const noexcept;
  value_type reset() noexcept;

  // ::bvar::detail::ReducerSampler要求的正反操作器
  Op op() const noexcept;
  InvOp inv_op() const noexcept;

 private:
  // ::bvar::Variable要求的单值展示接口
  virtual void describe(::std::ostream& os, bool) const noexcept override;

  ::babylon::ConcurrentMaxer _maxer;
  sampler_type* _sampler {nullptr};
};

// ::bvar::IntRecorder的替换实现
class BvarIntRecorder : public ::bvar::Variable {
 public:
  // ::bvar::Window要求的必要定义
  using value_type = ::bvar::Stat;
  using Op = ::bvar::IntRecorder::AddStat;
  using InvOp = ::bvar::IntRecorder::MinusStat;
  using sampler_type =
      ::bvar::detail::ReducerSampler<BvarIntRecorder, value_type, Op, InvOp>;

  BvarIntRecorder() noexcept = default;
  BvarIntRecorder(BvarIntRecorder&&) = delete;
  BvarIntRecorder(const BvarIntRecorder&) = delete;
  BvarIntRecorder& operator=(BvarIntRecorder&&) = delete;
  BvarIntRecorder& operator=(const BvarIntRecorder&) = delete;
  ~BvarIntRecorder() noexcept;

  // 计数接口
  inline BvarIntRecorder& operator<<(ssize_t value) noexcept;

  // ::bvar::Window要求的获取采样器接口
  sampler_type* get_sampler() noexcept;

  // ::bvar::detail::ReducerSampler要求的汇聚接口
  // 具备逆运算时实现get_value，否则实现reset
  value_type get_value() const noexcept;
  value_type reset() noexcept;

  // ::bvar::detail::ReducerSampler要求的正反操作器
  Op op() const noexcept;
  InvOp inv_op() const noexcept;

 private:
  // ::bvar::Variable要求的单值展示接口
  virtual void describe(::std::ostream& os, bool) const noexcept override;

  ::babylon::ConcurrentSummer _summer;
  sampler_type* _sampler {nullptr};
};

// ::bvar::detail::IntRecorder的替换实现
class BvarPercentile {
 public:
  // 支持加入Window所需的类型定义
  using value_type = ::bvar::detail::GlobalPercentileSamples;
  using Op = ::bvar::detail::Percentile::AddPercentileSamples;
  using InvOp = ::bvar::detail::VoidOp;
  using sampler_type =
      ::bvar::detail::ReducerSampler<BvarPercentile, value_type, Op, InvOp>;

  // 可默认构造，不能移动和拷贝
  BvarPercentile() noexcept = default;
  BvarPercentile(BvarPercentile&&) = delete;
  BvarPercentile(const BvarPercentile&) = delete;
  BvarPercentile& operator=(BvarPercentile&&) = delete;
  BvarPercentile& operator=(const BvarPercentile&) = delete;
  ~BvarPercentile() noexcept;

  // 计数操作
  inline BvarPercentile& operator<<(uint32_t value) noexcept;

  // ::bvar::Window要求的获取采样器接口
  sampler_type* get_sampler() noexcept;

  // ::bvar::detail::ReducerSampler要求的汇聚接口
  // 具备逆运算时实现get_value，否则实现reset
  value_type get_value() const noexcept;
  value_type reset() noexcept;

  // ::bvar::detail::ReducerSampler要求的正反操作器
  Op op() const noexcept;
  InvOp inv_op() const noexcept;

 private:
  static void merge_into_global_samples(
      size_t index, const ::babylon::ConcurrentSampler::SampleBucket& bucket,
      ::bvar::detail::GlobalPercentileSamples& global_samples);

  ::babylon::ConcurrentSampler _concurrent_sampler;
  sampler_type* _sampler {nullptr};
};

// ::bvar::LatencyRecorder的替换实现
class BvarLatencyRecorder {
 public:
  BvarLatencyRecorder() noexcept;
  BvarLatencyRecorder(BvarLatencyRecorder&&) = delete;
  BvarLatencyRecorder(const BvarLatencyRecorder&) = delete;
  BvarLatencyRecorder& operator=(BvarLatencyRecorder&&) = delete;
  BvarLatencyRecorder& operator=(const BvarLatencyRecorder&) = delete;
  ~BvarLatencyRecorder() noexcept;

  explicit BvarLatencyRecorder(time_t window_size) noexcept;
  explicit BvarLatencyRecorder(const ::butil::StringPiece& prefix) noexcept;
  BvarLatencyRecorder(const ::butil::StringPiece& prefix,
                      time_t window_size) noexcept;
  BvarLatencyRecorder(const ::butil::StringPiece& prefix1,
                      const ::butil::StringPiece& prefix2) noexcept;
  BvarLatencyRecorder(const ::butil::StringPiece& prefix1,
                      const ::butil::StringPiece& prefix2,
                      time_t window_size) noexcept;

  int expose(const ::butil::StringPiece& prefix) noexcept;
  int expose(const ::butil::StringPiece& prefix1,
             const ::butil::StringPiece& prefix2) noexcept;
  void hide() noexcept;

  BvarLatencyRecorder& operator<<(uint32_t latency) noexcept;

  int64_t latency(time_t window_size) const noexcept;
  int64_t latency() const noexcept;
  ::bvar::Vector<int64_t, 4> latency_percentiles() const noexcept;
  int64_t max_latency() const noexcept;
  int64_t count() const noexcept;
  int64_t qps(time_t window_size) const noexcept;
  int64_t qps() const noexcept;
  int64_t latency_percentile(double ratio) const noexcept;

  const ::std::string& latency_name() const noexcept;
  const ::std::string& latency_percentiles_name() const noexcept;
  const ::std::string& latency_cdf_name() const noexcept;
  const ::std::string& max_latency_name() const noexcept;
  const ::std::string& count_name() const noexcept;
  const ::std::string& qps_name() const noexcept;

 private:
  using GlobalPercentileSamples = ::bvar::detail::GlobalPercentileSamples;
  using CombinedPercentileSamples = ::bvar::detail::PercentileSamples<1022>;
  using RecorderWindow =
      ::bvar::Window<BvarIntRecorder, ::bvar::SERIES_IN_SECOND>;
  using MaxWindow = ::bvar::Window<BvarMaxer, ::bvar::SERIES_IN_SECOND>;
  using PercentileWindow =
      ::bvar::Window<BvarPercentile, ::bvar::SERIES_IN_SECOND>;
  template <typename T, size_t N>
  using Vector = ::bvar::Vector<T, N>;
  template <typename T>
  using PassiveStatus = ::bvar::PassiveStatus<T>;

  class CDF : public ::bvar::Variable {
   public:
    explicit CDF(PercentileWindow* w) noexcept;
    ~CDF() noexcept;
    void describe(::std::ostream& os, bool quote_string) const noexcept;
    int describe_series(::std::ostream& os,
                        const ::bvar::SeriesOptions& options) const noexcept;

   private:
    PercentileWindow* _w;
  };

  static constexpr ::bvar::DisplayFilter DISPLAY_ON_HTML =
      ::bvar::DISPLAY_ON_HTML;
  static constexpr ::bvar::DisplayFilter DISPLAY_ON_PLAIN_TEXT =
      ::bvar::DISPLAY_ON_PLAIN_TEXT;

  static ::std::unique_ptr<CombinedPercentileSamples> combine(
      PercentileWindow* w);
  static Vector<int64_t, 4> get_percentiles(void* arg);
  static uint32_t get_percentile_p1(void* arg);
  static uint32_t get_percentile_p2(void* arg);
  static uint32_t get_percentile_p3(void* arg);
  template <size_t NUMERATOR, size_t DENOMINATOR>
  static uint32_t get_percentile(void* arg);
  static uint64_t get_window_recorder_qps(void* args);
  static uint64_t get_recorder_count(void* args);

  BvarIntRecorder _latency;
  BvarMaxer _max_latency;
  BvarPercentile _latency_percentile;

  RecorderWindow _latency_window;
  MaxWindow _max_latency_window;
  PassiveStatus<uint64_t> _count;
  PassiveStatus<uint64_t> _qps;
  PercentileWindow _latency_percentile_window;
  PassiveStatus<uint32_t> _latency_p1;
  PassiveStatus<uint32_t> _latency_p2;
  PassiveStatus<uint32_t> _latency_p3;
  PassiveStatus<uint32_t> _latency_999;
  PassiveStatus<uint32_t> _latency_9999;
  CDF _latency_cdf;
  PassiveStatus<Vector<int64_t, 4>> _latency_percentiles;
};

////////////////////////////////////////////////////////////////////////////////
// BvarAdder begin
inline BvarAdder& BvarAdder::operator<<(value_type value) noexcept {
  _adder << value;
  return *this;
}
// BvarAdder end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// BvarMaxer begin
inline BvarMaxer& BvarMaxer::operator<<(value_type value) noexcept {
  _maxer << value;
  return *this;
}
// BvarMaxer end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// BvarIntRecorder begin
inline BvarIntRecorder& BvarIntRecorder::operator<<(ssize_t value) noexcept {
  _summer << value;
  return *this;
}
// BvarIntRecorder end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// BvarPercentile begin
inline BvarPercentile& BvarPercentile::operator<<(uint32_t value) noexcept {
  _concurrent_sampler << value;
  return *this;
}
// BvarPercentile end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// BvarLatencyRecorder begin
template <size_t NUMERATOR, size_t DENOMINATOR>
uint32_t BvarLatencyRecorder::get_percentile(void* arg) {
  return combine(reinterpret_cast<PercentileWindow*>(arg))
      ->get_number(static_cast<double>(NUMERATOR) / DENOMINATOR);
}
// BvarLatencyRecorder end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END
