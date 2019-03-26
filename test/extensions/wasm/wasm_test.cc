#include <stdio.h>

#include "common/event/dispatcher_impl.h"
#include "common/stats/isolated_store_impl.h"

#include "extensions/common/wasm/wasm.h"

#include "test/mocks/upstream/mocks.h"
#include "test/test_common/environment.h"
#include "test/test_common/utility.h"

#include "absl/types/optional.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::Eq;

namespace Envoy {
namespace Extensions {
namespace Wasm {

class TestContext : public Extensions::Common::Wasm::Context {
public:
  TestContext(Extensions::Common::Wasm::Wasm* wasm) : Extensions::Common::Wasm::Context(wasm) {}
  ~TestContext() override {}
  MOCK_METHOD2(scriptLog, void(spdlog::level::level_enum level, absl::string_view message));
  MOCK_METHOD1(setTickPeriodMilliseconds, void(uint32_t tick_period_milliseconds));
};

class WasmTestCppRust : public TestBaseWithParam<std::string> {};

INSTANTIATE_TEST_SUITE_P(SourceLanguages, WasmTestCppRust, testing::Values("cpp", "rust"));

TEST_P(WasmTestCppRust, Logging) {
  Stats::IsolatedStoreImpl stats_store;
  Api::ApiPtr api = Api::createApiForTest(stats_store);
  Upstream::MockClusterManager cluster_manager;
  Event::DispatcherImpl dispatcher(*api);
  auto wasm = std::make_unique<Extensions::Common::Wasm::Wasm>("envoy.wasm.vm.wavm", "", "",
                                                               cluster_manager, dispatcher);
  EXPECT_NE(wasm, nullptr);
  const auto code =
      TestEnvironment::readFileToStringForTest(TestEnvironment::substitute(absl::StrCat(
          "{{ test_rundir }}/test/extensions/wasm/test_data/logging_", GetParam(), ".wasm")));
  EXPECT_FALSE(code.empty());
  auto context = std::make_unique<TestContext>(wasm.get());

  EXPECT_CALL(*context, scriptLog(spdlog::level::warn, Eq("warn configure-test")));
  EXPECT_CALL(*context, scriptLog(spdlog::level::trace, Eq("test trace logging")));
  EXPECT_CALL(*context, scriptLog(spdlog::level::debug, Eq("test debug logging")));
  EXPECT_CALL(*context, scriptLog(spdlog::level::err, Eq("test error logging")));
  EXPECT_CALL(*context, scriptLog(spdlog::level::info, Eq("test tick logging")));

  EXPECT_TRUE(wasm->initialize(code, "<test>", false));
  // NB: Must be done after initialize has created the context.
  wasm->setGeneralContext(std::move(context));
  wasm->configure("configure-test");
  wasm->start();
  wasm->tickHandler();
}

TEST(WasmTest, BadSignature) {
  Stats::IsolatedStoreImpl stats_store;
  Api::ApiPtr api = Api::createApiForTest(stats_store);
  Upstream::MockClusterManager cluster_manager;
  Event::DispatcherImpl dispatcher(*api);
  auto wasm = std::make_shared<Extensions::Common::Wasm::Wasm>("envoy.wasm.vm.wavm", "", "",
                                                               cluster_manager, dispatcher);
  EXPECT_NE(wasm, nullptr);
  const auto code = TestEnvironment::readFileToStringForTest(TestEnvironment::substitute(
      "{{ test_rundir }}/test/extensions/wasm/test_data/bad_signature_cpp.wasm"));
  EXPECT_FALSE(code.empty());
  auto context = std::make_unique<TestContext>(wasm.get());
  EXPECT_THROW_WITH_MESSAGE(wasm->initialize(code, "<test>", false),
                            Extensions::Common::Wasm::WasmVmException,
                            "Bad function signature for: _proxy_onConfigure");
}

TEST(WasmTest, Segv) {
  Stats::IsolatedStoreImpl stats_store;
  Api::ApiPtr api = Api::createApiForTest(stats_store);
  Upstream::MockClusterManager cluster_manager;
  Event::DispatcherImpl dispatcher(*api);
  auto wasm = std::make_shared<Extensions::Common::Wasm::Wasm>("envoy.wasm.vm.wavm", "", "",
                                                               cluster_manager, dispatcher);
  EXPECT_NE(wasm, nullptr);
  const auto code = TestEnvironment::readFileToStringForTest(TestEnvironment::substitute(
      "{{ test_rundir }}/test/extensions/wasm/test_data/segv_cpp.wasm"));
  EXPECT_FALSE(code.empty());
  auto context = std::make_unique<TestContext>(wasm.get());
  EXPECT_CALL(*context, scriptLog(spdlog::level::err, Eq("before badptr")));
  EXPECT_TRUE(wasm->initialize(code, "<test>", false));
  wasm->setGeneralContext(std::move(context));
  EXPECT_THROW_WITH_MESSAGE(wasm->start(), Extensions::Common::Wasm::WasmException,
                            "emscripten llvm_trap");
}

TEST(WasmTest, DivByZero) {
  Stats::IsolatedStoreImpl stats_store;
  Api::ApiPtr api = Api::createApiForTest(stats_store);
  Upstream::MockClusterManager cluster_manager;
  Event::DispatcherImpl dispatcher(*api);
  auto wasm = std::make_shared<Extensions::Common::Wasm::Wasm>("envoy.wasm.vm.wavm", "", "",
                                                               cluster_manager, dispatcher);
  EXPECT_NE(wasm, nullptr);
  const auto code = TestEnvironment::readFileToStringForTest(TestEnvironment::substitute(
      "{{ test_rundir }}/test/extensions/wasm/test_data/segv_cpp.wasm"));
  EXPECT_FALSE(code.empty());
  auto context = std::make_unique<TestContext>(wasm.get());
  EXPECT_CALL(*context, scriptLog(spdlog::level::err, Eq("before div by zero")));
  EXPECT_TRUE(wasm->initialize(code, "<test>", false));
  wasm->setGeneralContext(std::move(context));
  wasm->wasmVm()->start(wasm->generalContext());
  EXPECT_THROW_WITH_REGEX(wasm->generalContext()->onLog(), Extensions::Common::Wasm::WasmException,
                          "wavm.integerDivideByZeroOrOverflow.*");
}

TEST(WasmTest, EmscriptenVersion) {
  Stats::IsolatedStoreImpl stats_store;
  Api::ApiPtr api = Api::createApiForTest(stats_store);
  Upstream::MockClusterManager cluster_manager;
  Event::DispatcherImpl dispatcher(*api);
  auto wasm = std::make_shared<Extensions::Common::Wasm::Wasm>("envoy.wasm.vm.wavm", "", "",
                                                               cluster_manager, dispatcher);
  EXPECT_NE(wasm, nullptr);
  const auto code = TestEnvironment::readFileToStringForTest(TestEnvironment::substitute(
      "{{ test_rundir }}/test/extensions/wasm/test_data/segv_cpp.wasm"));
  EXPECT_FALSE(code.empty());
  auto context = std::make_unique<TestContext>(wasm.get());
  EXPECT_TRUE(wasm->initialize(code, "<test>", false));
  uint32_t major = 9, minor = 9, abi_major = 9, abi_minor = 9;
  EXPECT_TRUE(wasm->getEmscriptenVersion(&major, &minor, &abi_major, &abi_minor));
  EXPECT_EQ(major, 0);
  EXPECT_EQ(minor, 0);
  EXPECT_EQ(abi_major, 0);
  EXPECT_EQ(abi_minor, 1);
}

TEST(WasmTest, IntrinsicGlobals) {
  Stats::IsolatedStoreImpl stats_store;
  Api::ApiPtr api = Api::createApiForTest(stats_store);
  Upstream::MockClusterManager cluster_manager;
  Event::DispatcherImpl dispatcher(*api);
  auto wasm = std::make_shared<Extensions::Common::Wasm::Wasm>("envoy.wasm.vm.wavm", "", "",
                                                               cluster_manager, dispatcher);
  EXPECT_NE(wasm, nullptr);
  const auto code = TestEnvironment::readFileToStringForTest(TestEnvironment::substitute(
      "{{ test_rundir }}/test/extensions/wasm/test_data/emscripten_cpp.wasm"));
  EXPECT_FALSE(code.empty());
  auto context = std::make_unique<TestContext>(wasm.get());
  EXPECT_CALL(*context, scriptLog(spdlog::level::debug, Eq("pow(2.0, 2.0) 4.000000")));
  EXPECT_CALL(*context, scriptLog(spdlog::level::info, Eq("NaN nan")));
  EXPECT_CALL(*context, scriptLog(spdlog::level::warn, Eq("inf inf")));
  EXPECT_TRUE(wasm->initialize(code, "<test>", false));
  wasm->setGeneralContext(std::move(context));
  wasm->start();
}

// The asm2wasm.wasm file uses operations which would require the asm2wasm Emscripten module *if*
// em++ is invoked with the trap mode "clamp". See
// https://emscripten.org/docs/compiling/WebAssembly.html This test demonstrates that the asm2wasm
// module is not required with the trap mode is set to "allow". Note: future WASM standards will
// change this behavior by providing non-trapping instructions, but in the mean time we support the
// default Emscripten behavior.
TEST(WasmTest, Asm2Wasm) {
  Stats::IsolatedStoreImpl stats_store;
  Api::ApiPtr api = Api::createApiForTest(stats_store);
  Upstream::MockClusterManager cluster_manager;
  Event::DispatcherImpl dispatcher(*api);
  auto wasm = std::make_shared<Extensions::Common::Wasm::Wasm>("envoy.wasm.vm.wavm", "", "",
                                                               cluster_manager, dispatcher);
  EXPECT_NE(wasm, nullptr);
  const auto code = TestEnvironment::readFileToStringForTest(TestEnvironment::substitute(
      "{{ test_rundir }}/test/extensions/wasm/test_data/asm2wasm_cpp.wasm"));
  EXPECT_FALSE(code.empty());
  auto context = std::make_unique<TestContext>(wasm.get());
  EXPECT_CALL(*context, scriptLog(spdlog::level::info, Eq("out 0 0 0")));
  EXPECT_TRUE(wasm->initialize(code, "<test>", false));
  wasm->setGeneralContext(std::move(context));
  wasm->start();
}

} // namespace Wasm
} // namespace Extensions
} // namespace Envoy
