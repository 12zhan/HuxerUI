#include "runtime_test_support.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

namespace huxerui::test {
namespace {

class TestClipboard final : public PlatformClipboard {
public:
  std::optional<std::string> ReadText() override {
    ++read_count;
    return text;
  }

  bool WriteText(std::string_view value) override {
    ++write_count;
    text = std::string(value);
    return write_succeeds;
  }

  std::optional<std::string> text;
  int read_count = 0;
  int write_count = 0;
  bool write_succeeds = true;
};

std::shared_ptr<Clipboard> clipboard_service;

View ClipboardApp() {
  clipboard_service = UseService<Clipboard>();
  return Text("Clipboard");
}

void ResetClipboardService() {
  clipboard_service.reset();
}

} // namespace

static_assert(!std::is_copy_constructible_v<Clipboard>);
static_assert(!std::is_move_constructible_v<Clipboard>);

TEST_CASE("RuntimeInstallsClipboardServiceAndDelegatesTextOperations") {
  ResetClipboardService();
  TestClipboard platform_clipboard;
  platform_clipboard.text = "initial";
  TestPlatform platform;
  platform.platform_clipboard = &platform_clipboard;
  Runtime runtime(ClipboardApp, platform);
  runtime.BuildFrame();

  REQUIRE(clipboard_service);
  REQUIRE(clipboard_service->IsAvailable());
  REQUIRE(clipboard_service->ReadText() == std::optional<std::string>{"initial"});
  REQUIRE(platform_clipboard.read_count == 1);

  REQUIRE(clipboard_service->WriteText("updated"));
  REQUIRE(platform_clipboard.text == std::optional<std::string>{"updated"});
  REQUIRE(platform_clipboard.write_count == 1);
}

TEST_CASE("UnsupportedClipboardServiceReturnsUnavailableResults") {
  ResetClipboardService();
  TestPlatform platform;
  Runtime runtime(ClipboardApp, platform);
  runtime.BuildFrame();

  REQUIRE(clipboard_service);
  REQUIRE_FALSE(clipboard_service->IsAvailable());
  REQUIRE_FALSE(clipboard_service->ReadText().has_value());
  REQUIRE_FALSE(clipboard_service->WriteText("text"));
}

TEST_CASE("ClipboardRejectsInvalidUtf8BeforeCallingThePlatform") {
  ResetClipboardService();
  TestClipboard platform_clipboard;
  TestPlatform platform;
  platform.platform_clipboard = &platform_clipboard;
  Runtime runtime(ClipboardApp, platform);
  runtime.BuildFrame();

  const std::string invalid_utf8{"\xC3\x28", 2};
  REQUIRE_THROWS_AS(clipboard_service->WriteText(invalid_utf8), std::invalid_argument);
  REQUIRE(platform_clipboard.write_count == 0);

  platform_clipboard.text = invalid_utf8;
  REQUIRE_FALSE(clipboard_service->ReadText().has_value());
  REQUIRE(platform_clipboard.read_count == 1);
}

TEST_CASE("ClipboardServiceDisconnectsWhenRuntimeIsDestroyed") {
  ResetClipboardService();
  TestClipboard platform_clipboard;
  TestPlatform platform;
  platform.platform_clipboard = &platform_clipboard;
  {
    Runtime runtime(ClipboardApp, platform);
    runtime.BuildFrame();
    REQUIRE(clipboard_service->IsAvailable());
  }

  REQUIRE_FALSE(clipboard_service->IsAvailable());
  REQUIRE_FALSE(clipboard_service->ReadText().has_value());
  REQUIRE_FALSE(clipboard_service->WriteText("ignored"));
  REQUIRE(platform_clipboard.read_count == 0);
  REQUIRE(platform_clipboard.write_count == 0);
}

} // namespace huxerui::test
