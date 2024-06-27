#include "recorder.h"

namespace bvar {
DECLARE_int32(bvar_latency_p1);
DECLARE_int32(bvar_latency_p2);
DECLARE_int32(bvar_latency_p3);
} // namespace bvar

BABYLON_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
// BvarAdder::SeriesSampler begin
BvarAdder::SeriesSampler::SeriesSampler(BvarAdder* owner, const Op& op) noexcept
    : _owner(owner), _series(op) {}

void BvarAdder::SeriesSampler::take_sample() noexcept {
  _series.append(_owner->get_value());
}

void BvarAdder::SeriesSampler::describe(::std::ostream& os) noexcept {
  _series.describe(os, nullptr);
}
// BvarAdder::SeriesSampler end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// BvarAdder begin
BvarAdder::~BvarAdder() noexcept {
  hide();
  if (_sampler) {
    _sampler->destroy();
  }
  if (_series_sampler) {
    _series_sampler->destroy();
  }
}

BvarAdder::BvarAdder(const ::butil::StringPiece& name) noexcept {
  expose(name);
};

BvarAdder::BvarAdder(const ::butil::StringPiece& prefix,
                     const ::butil::StringPiece& name) noexcept {
  expose_as(prefix, name);
};

BvarAdder::sampler_type* BvarAdder::get_sampler() noexcept {
  if (_sampler == nullptr) {
    _sampler = new sampler_type(this);
    _sampler->schedule();
  }
  return _sampler;
}

BvarAdder::value_type BvarAdder::get_value() const noexcept {
  return _adder.value();
}

BvarAdder::value_type BvarAdder::reset() noexcept {
  LOG_EVERY_SECOND(FATAL) << "sampler should never call this";
  return get_value();
}

BvarAdder::Op BvarAdder::op() const noexcept {
  return Op();
}

BvarAdder::InvOp BvarAdder::inv_op() const noexcept {
  return InvOp();
}

void BvarAdder::describe(std::ostream& os, bool) const noexcept {
  os << get_value();
}

int BvarAdder::describe_series(
    ::std::ostream& os, const ::bvar::SeriesOptions& options) const noexcept {
  if (_series_sampler == nullptr) {
    return 1;
  }
  _series_sampler->describe(os);
  return 0;
}

int BvarAdder::expose_impl(const ::butil::StringPiece& prefix,
                           const ::butil::StringPiece& name,
                           ::bvar::DisplayFilter display_filter) noexcept {
  auto rc = ::bvar::Variable::expose_impl(prefix, name, display_filter);
  if (rc == 0 && _series_sampler == nullptr) {
    _series_sampler = new SeriesSampler(this, op());
    _series_sampler->schedule();
  }
  return rc;
}
// BvarAdder end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// BvarMaxer begin
BvarMaxer::~BvarMaxer() noexcept {
  hide();
  if (_sampler) {
    _sampler->destroy();
  }
}

BvarMaxer::sampler_type* BvarMaxer::get_sampler() noexcept {
  if (_sampler == nullptr) {
    _sampler = new sampler_type(this);
    _sampler->schedule();
  }
  return _sampler;
}

BvarMaxer::value_type BvarMaxer::get_value() const noexcept {
  return _maxer.value();
}

BvarMaxer::value_type BvarMaxer::reset() noexcept {
  auto result = _maxer.value();
  _maxer.reset();
  return result;
}

BvarMaxer::Op BvarMaxer::op() const noexcept {
  return Op();
}

BvarMaxer::InvOp BvarMaxer::inv_op() const noexcept {
  return InvOp();
}

void BvarMaxer::describe(std::ostream& os, bool) const noexcept {
  os << _maxer.value();
}
// BvarMaxer end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// BvarIntRecorder begin
BvarIntRecorder::~BvarIntRecorder() noexcept {
  hide();
  if (_sampler) {
    _sampler->destroy();
  }
}

BvarIntRecorder::sampler_type* BvarIntRecorder::get_sampler() noexcept {
  if (_sampler == nullptr) {
    _sampler = new sampler_type(this);
    _sampler->schedule();
  }
  return _sampler;
}

BvarIntRecorder::value_type BvarIntRecorder::get_value() const noexcept {
  auto summary = _summer.value();
  return value_type {summary.sum, static_cast<ssize_t>(summary.num)};
}

BvarIntRecorder::value_type BvarIntRecorder::reset() noexcept {
  LOG_EVERY_SECOND(FATAL) << "sampler should never call this";
  return value_type {};
}

BvarIntRecorder::Op BvarIntRecorder::op() const noexcept {
  return Op();
}

BvarIntRecorder::InvOp BvarIntRecorder::inv_op() const noexcept {
  return InvOp();
}

void BvarIntRecorder::describe(std::ostream& os, bool) const noexcept {
  os << get_value();
}
// BvarIntRecorder end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// BvarPercentile begin
BvarPercentile::~BvarPercentile() noexcept {
  if (_sampler) {
    _sampler->destroy();
  }
}

BvarPercentile::sampler_type* BvarPercentile::get_sampler() noexcept {
  if (_sampler == nullptr) {
    _sampler = new sampler_type(this);
    _sampler->schedule();
  }
  return _sampler;
}

BvarPercentile::value_type BvarPercentile::get_value() const noexcept {
  LOG_EVERY_SECOND(FATAL) << "sampler should never call this";
  return value_type();
}

BvarPercentile::value_type BvarPercentile::reset() noexcept {
  constexpr static size_t SAMPLE_SIZE = value_type::SAMPLE_SIZE;
  value_type result;
  _concurrent_sampler.for_each(
      [&](size_t index,
          const ::babylon::ConcurrentSampler::SampleBucket& bucket) {
        merge_into_global_samples(index, bucket, result);
        auto capacity = _concurrent_sampler.bucket_capacity(index);
        auto num_added = bucket.record_num.load(::std::memory_order_relaxed);
        if (capacity < SAMPLE_SIZE && num_added > capacity) {
          capacity = ::std::min<size_t>(SAMPLE_SIZE, num_added * 1.5);
          _concurrent_sampler.set_bucket_capacity(index, capacity);
        }
      });
  _concurrent_sampler.reset();
  return result;
}

BvarPercentile::Op BvarPercentile::op() const noexcept {
  return Op();
}

BvarPercentile::InvOp BvarPercentile::inv_op() const noexcept {
  return InvOp();
}
// BvarPercentile end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// BvarLatencyRecorder::CDF begin
BvarLatencyRecorder::CDF::CDF(PercentileWindow* w) noexcept : _w {w} {}

BvarLatencyRecorder::CDF::~CDF() noexcept {
  hide();
}

void BvarLatencyRecorder::CDF::describe(std::ostream& os, bool) const noexcept {
  os << "\"click to view\"";
}

int BvarLatencyRecorder::CDF::describe_series(
    ::std::ostream& os, const ::bvar::SeriesOptions& options) const noexcept {
  if (_w == NULL) {
    return 1;
  }
  if (options.test_only) {
    return 0;
  }
  std::unique_ptr<CombinedPercentileSamples> cb(new CombinedPercentileSamples);
  std::vector<GlobalPercentileSamples> buckets;
  _w->get_samples(&buckets);
  for (size_t i = 0; i < buckets.size(); ++i) {
    cb->combine_of(buckets.begin(), buckets.end());
  }
  std::pair<int, int> values[20];
  size_t n = 0;
  for (int i = 1; i < 10; ++i) {
    values[n++] = std::make_pair(i * 10, cb->get_number(i * 0.1));
  }
  for (int i = 91; i < 100; ++i) {
    values[n++] = std::make_pair(i, cb->get_number(i * 0.01));
  }
  values[n++] = std::make_pair(100, cb->get_number(0.999));
  values[n++] = std::make_pair(101, cb->get_number(0.9999));
  CHECK_EQ(n, arraysize(values));
  os << "{\"label\":\"cdf\",\"data\":[";
  for (size_t i = 0; i < n; ++i) {
    if (i) {
      os << ',';
    }
    os << '[' << values[i].first << ',' << values[i].second << ']';
  }
  os << "]}";
  return 0;
}
// BvarLatencyRecorder::CDF end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// BvarLatencyRecorder begin
BvarLatencyRecorder::BvarLatencyRecorder() noexcept
    : BvarLatencyRecorder {-1} {}

BvarLatencyRecorder::~BvarLatencyRecorder() noexcept {
  hide();
}

BvarLatencyRecorder::BvarLatencyRecorder(time_t window_size) noexcept
    : _latency_window(&_latency, window_size),
      _max_latency_window(&_max_latency, window_size),
      _count(get_recorder_count, &_latency),
      _qps(get_window_recorder_qps, &_latency_window),
      _latency_percentile_window(&_latency_percentile, window_size),
      _latency_p1(get_percentile_p1, &_latency_percentile_window),
      _latency_p2(get_percentile_p2, &_latency_percentile_window),
      _latency_p3(get_percentile_p3, &_latency_percentile_window),
      _latency_999(get_percentile<999, 1000>, &_latency_percentile_window),
      _latency_9999(get_percentile<9999, 10000>, &_latency_percentile_window),
      _latency_cdf(&_latency_percentile_window),
      _latency_percentiles(get_percentiles, &_latency_percentile_window) {}

BvarLatencyRecorder::BvarLatencyRecorder(
    const ::butil::StringPiece& prefix) noexcept
    : BvarLatencyRecorder {} {
  expose(prefix);
}

BvarLatencyRecorder::BvarLatencyRecorder(const ::butil::StringPiece& prefix,
                                         time_t window_size) noexcept
    : BvarLatencyRecorder {window_size} {
  expose(prefix);
}

BvarLatencyRecorder::BvarLatencyRecorder(
    const ::butil::StringPiece& prefix1,
    const ::butil::StringPiece& prefix2) noexcept
    : BvarLatencyRecorder {} {
  expose(prefix1, prefix2);
}

BvarLatencyRecorder::BvarLatencyRecorder(const ::butil::StringPiece& prefix1,
                                         const ::butil::StringPiece& prefix2,
                                         time_t window_size) noexcept
    : BvarLatencyRecorder {window_size} {
  expose(prefix1, prefix2);
}

int BvarLatencyRecorder::expose(const ::butil::StringPiece& prefix) noexcept {
  return expose(::butil::StringPiece(), prefix);
}

int BvarLatencyRecorder::expose(const ::butil::StringPiece& prefix1,
                                const ::butil::StringPiece& prefix2) noexcept {
  if (prefix2.empty()) {
    LOG(ERROR) << "Parameter[prefix2] is empty";
    return -1;
  }
  ::butil::StringPiece prefix = prefix2;
  // User may add "_latency" as the suffix, remove it.
  if (prefix.ends_with("latency") || prefix.ends_with("Latency")) {
    prefix.remove_suffix(7);
    if (prefix.empty()) {
      LOG(ERROR) << "Invalid prefix2=" << prefix2;
      return -1;
    }
  }
  ::std::string tmp;
  if (!prefix1.empty()) {
    tmp.reserve(prefix1.size() + prefix.size() + 1);
    tmp.append(prefix1.data(), prefix1.size());
    tmp.push_back('_'); // prefix1 ending with _ is good.
    tmp.append(prefix.data(), prefix.size());
    prefix = tmp;
  }

  if (_latency_window.expose_as(prefix, "latency") != 0) {
    return -1;
  }
  if (_max_latency_window.expose_as(prefix, "max_latency") != 0) {
    return -1;
  }
  if (_count.expose_as(prefix, "count") != 0) {
    return -1;
  }
  if (_qps.expose_as(prefix, "qps") != 0) {
    return -1;
  }
  char namebuf[32];
  snprintf(namebuf, sizeof(namebuf), "latency_%d",
           (int)::bvar::FLAGS_bvar_latency_p1);
  if (_latency_p1.expose_as(prefix, namebuf, DISPLAY_ON_PLAIN_TEXT) != 0) {
    return -1;
  }
  snprintf(namebuf, sizeof(namebuf), "latency_%d",
           (int)::bvar::FLAGS_bvar_latency_p2);
  if (_latency_p2.expose_as(prefix, namebuf, DISPLAY_ON_PLAIN_TEXT) != 0) {
    return -1;
  }
  snprintf(namebuf, sizeof(namebuf), "latency_%u",
           (int)::bvar::FLAGS_bvar_latency_p3);
  if (_latency_p3.expose_as(prefix, namebuf, DISPLAY_ON_PLAIN_TEXT) != 0) {
    return -1;
  }
  if (_latency_999.expose_as(prefix, "latency_999", DISPLAY_ON_PLAIN_TEXT) !=
      0) {
    return -1;
  }
  if (_latency_9999.expose_as(prefix, "latency_9999") != 0) {
    return -1;
  }
  if (_latency_cdf.expose_as(prefix, "latency_cdf", DISPLAY_ON_HTML) != 0) {
    return -1;
  }
  if (_latency_percentiles.expose_as(prefix, "latency_percentiles",
                                     DISPLAY_ON_HTML) != 0) {
    return -1;
  }
  snprintf(namebuf, sizeof(namebuf), "%d%%,%d%%,%d%%,99.9%%",
           (int)::bvar::FLAGS_bvar_latency_p1,
           (int)::bvar::FLAGS_bvar_latency_p2,
           (int)::bvar::FLAGS_bvar_latency_p3);
  CHECK_EQ(0, _latency_percentiles.set_vector_names(namebuf));
  return 0;
}

void BvarLatencyRecorder::hide() noexcept {
  _latency_window.hide();
  _max_latency_window.hide();
  _count.hide();
  _qps.hide();
  _latency_p1.hide();
  _latency_p2.hide();
  _latency_p3.hide();
  _latency_999.hide();
  _latency_9999.hide();
  _latency_cdf.hide();
  _latency_percentiles.hide();
}

BvarLatencyRecorder& BvarLatencyRecorder::operator<<(
    uint32_t latency) noexcept {
  _latency << latency;
  _max_latency << latency;
  _latency_percentile << latency;
  return *this;
}

int64_t BvarLatencyRecorder::latency(time_t window_size) const noexcept {
  return _latency_window.get_value(window_size).get_average_int();
}

int64_t BvarLatencyRecorder::latency() const noexcept {
  return _latency_window.get_value().get_average_int();
}

int64_t BvarLatencyRecorder::latency_percentile(double ratio) const noexcept {
  std::unique_ptr<CombinedPercentileSamples> cb(
      combine((PercentileWindow*)&_latency_percentile_window));
  return cb->get_number(ratio);
}

int64_t BvarLatencyRecorder::max_latency() const noexcept {
  return _max_latency_window.get_value();
}

int64_t BvarLatencyRecorder::count() const noexcept {
  return _latency.get_value().num;
}

int64_t BvarLatencyRecorder::qps(time_t window_size) const noexcept {
  ::bvar::detail::Sample<::bvar::Stat> s;
  _latency_window.get_span(window_size, &s);
  // Use floating point to avoid overflow.
  if (s.time_us <= 0) {
    return 0;
  }
  return static_cast<int64_t>(round(s.data.num * 1000000.0 / s.time_us));
}

int64_t BvarLatencyRecorder::qps() const noexcept {
  return _qps.get_value();
}

::bvar::Vector<int64_t, 4> BvarLatencyRecorder::latency_percentiles()
    const noexcept {
  return get_percentiles(
      const_cast<PercentileWindow*>(&_latency_percentile_window));
}

const ::std::string& BvarLatencyRecorder::latency_name() const noexcept {
  return _latency_window.name();
}

const ::std::string& BvarLatencyRecorder::latency_percentiles_name()
    const noexcept {
  return _latency_percentiles.name();
}

const ::std::string& BvarLatencyRecorder::latency_cdf_name() const noexcept {
  return _latency_cdf.name();
}

const ::std::string& BvarLatencyRecorder::max_latency_name() const noexcept {
  return _max_latency_window.name();
}

const ::std::string& BvarLatencyRecorder::count_name() const noexcept {
  return _count.name();
}

const ::std::string& BvarLatencyRecorder::qps_name() const noexcept {
  return _qps.name();
}

::std::unique_ptr<BvarLatencyRecorder::CombinedPercentileSamples>
BvarLatencyRecorder::combine(PercentileWindow* w) {
  ::std::unique_ptr<CombinedPercentileSamples> cb(
      new CombinedPercentileSamples);
  std::vector<GlobalPercentileSamples> buckets;
  w->get_samples(&buckets);
  cb->combine_of(buckets.begin(), buckets.end());
  return cb;
}

::bvar::Vector<int64_t, 4> BvarLatencyRecorder::get_percentiles(void* arg) {
  auto cb = combine(reinterpret_cast<PercentileWindow*>(arg));
  Vector<int64_t, 4> result;
  result[0] = cb->get_number(::bvar::FLAGS_bvar_latency_p1 / 100.0);
  result[1] = cb->get_number(::bvar::FLAGS_bvar_latency_p2 / 100.0);
  result[2] = cb->get_number(::bvar::FLAGS_bvar_latency_p3 / 100.0);
  result[3] = cb->get_number(0.999);
  return result;
}

uint32_t BvarLatencyRecorder::get_percentile_p1(void* arg) {
  return combine(reinterpret_cast<PercentileWindow*>(arg))
      ->get_number(::bvar::FLAGS_bvar_latency_p1 / 100.0);
}

uint32_t BvarLatencyRecorder::get_percentile_p2(void* arg) {
  return combine(reinterpret_cast<PercentileWindow*>(arg))
      ->get_number(::bvar::FLAGS_bvar_latency_p2 / 100.0);
}

uint32_t BvarLatencyRecorder::get_percentile_p3(void* arg) {
  return combine(reinterpret_cast<PercentileWindow*>(arg))
      ->get_number(::bvar::FLAGS_bvar_latency_p3 / 100.0);
}

uint64_t BvarLatencyRecorder::get_window_recorder_qps(void* args) {
  ::bvar::detail::Sample<::bvar::Stat> sample;
  reinterpret_cast<RecorderWindow*>(args)->get_span(1, &sample);
  if (sample.time_us <= 0) {
    return 0;
  }
  return static_cast<uint64_t>(
      round(sample.data.num * 1000000.0 / sample.time_us));
}

uint64_t BvarLatencyRecorder::get_recorder_count(void* args) {
  return reinterpret_cast<BvarIntRecorder*>(args)->get_value().num;
}

BABYLON_NAMESPACE_END
