#pragma once

#include <atomic>
#include <chrono>
#include <list>
#include <mutex>
#include <vector>

#include "metrics.pb.h"

#include "prometheus/metric.h"

namespace prometheus {

namespace detail {
class CKMSQuantiles {
 public:
  struct Quantile {
    const double quantile;
    const double error;
    const double u;
    const double v;

    Quantile(double quantile, double error);
  };

 private:
  struct Item {
    /*const*/ double value;
    int g;
    /*const*/ int delta;

    explicit Item(double value, int lower_delta, int delta);
  };

 public:
  explicit CKMSQuantiles(const std::vector<Quantile>& quantiles);

  void insert(double value);
  double get(double q);
  void reset();

 private:
  double allowableError(int rank);
  bool insertBatch();
  void compress();

 private:
  const std::reference_wrapper<const std::vector<Quantile>> quantiles_;

  std::size_t count_;
  std::vector<Item> sample_;
  std::array<double, 500> buffer_;
  std::size_t buffer_count_;
};

class TimeWindowQuantiles {
  using Clock = std::chrono::steady_clock;

 public:
  TimeWindowQuantiles(const std::vector<CKMSQuantiles::Quantile>& quantiles,
                      Clock::duration max_age_seconds, int age_buckets);

  double get(double q);
  void insert(double value);

 private:
  CKMSQuantiles& rotate();

  const std::vector<CKMSQuantiles::Quantile>& quantiles_;
  std::vector<CKMSQuantiles> ckms_quantiles_;
  std::size_t current_bucket_;

  Clock::time_point last_rotation_;
  const Clock::duration rotation_interval_;
};
}  // namespace detail

class Summary : public Metric {
 public:
  using Quantiles = std::vector<detail::CKMSQuantiles::Quantile>;

  static const io::prometheus::client::MetricType metric_type =
      io::prometheus::client::SUMMARY;

  Summary(const Quantiles& quantiles,
          std::chrono::milliseconds max_age_seconds = std::chrono::seconds(60),
          int age_buckets = 5);

  void Observe(double value);

  io::prometheus::client::Metric Collect();

 private:
  const Quantiles quantiles_;

  std::mutex mutex_;

  double count_;
  double sum_;
  detail::TimeWindowQuantiles quantile_values_;
};
}  // namespace prometheus
