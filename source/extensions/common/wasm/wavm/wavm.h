#pragma once

#include <memory>

#include "extensions/common/wasm/wasm.h"

namespace Envoy {
namespace Extensions {
namespace Common {
namespace Wasm {
namespace Wavm {

std::unique_ptr<WasmVm> createWavm();

} // namespace Wavm
} // namespace Wasm
} // namespace Common
} // namespace Extensions
} // namespace Envoy
