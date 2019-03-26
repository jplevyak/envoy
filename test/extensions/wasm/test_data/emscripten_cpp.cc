// NOLINT(namespace-envoy)
#include <cmath>
#include <limits>
#include <string>

#include "proxy_wasm_intrinsics.h"

float gNan = std::nan("1");
float gInfinity = INFINITY;
float g1 = 1.0;
float g2 = 2.0;

extern "C" EMSCRIPTEN_KEEPALIVE void proxy_onStart() {
  g2 = g2 * g1;
  logDebug(std::string("pow(2.0, 2.0) ") + std::to_string(std::pow(g2, g2)));
  logInfo(std::string("NaN ") + std::to_string(gNan));
  // For some reason these return "inf nan":
  // logWarn("inf " + std::to_string(gInfinity));
  // logInfo("inf " + std::to_string(1.0/0.0));
  logWarn(std::string("inf ") + (std::isinf(gInfinity) ? "inf" : "nan"));
}
