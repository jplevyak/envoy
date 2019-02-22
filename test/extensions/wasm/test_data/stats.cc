// NOLINT(namespace-envoy)
#include <string>

#include "proxy_wasm_intrinsics.h"

extern "C" EMSCRIPTEN_KEEPALIVE void proxy_onStart() {
  auto c = defineMetric(MetricType::Counter, "test_counter");
  auto g = defineMetric(MetricType::Gauge, "test_gauges");
  auto h = defineMetric(MetricType::Histogram, "test_histogram");

  incrementMetric(c, 1);
  recordMetric(g, 2);
  recordMetric(h, 3);

  logTrace(std::string("get counter = ") + std::to_string(getMetric(c)));
  incrementMetric(c, 1);
  logDebug(std::string("get counter = ") + std::to_string(getMetric(c)));
  recordMetric(c, 3);
  logInfo(std::string("get counter = ") + std::to_string(getMetric(c)));
  logWarn(std::string("get gauge = ") + std::to_string(getMetric(g)));
  logError(std::string("get histogram = ") + std::to_string(getMetric(h)));
}
