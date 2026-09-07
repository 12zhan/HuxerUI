#include <catch2/catch_amalgamated.hpp>

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include <huxerui/stream.h>

#include "io/stream_internal.h"

namespace huxerui::test {

namespace {

class MemoryInputState final : public detail::InputStreamState {
public:
  explicit MemoryInputState(Bytes bytes) : bytes_(std::move(bytes)) {}

  IoResult<std::size_t> Read(std::span<std::byte> buffer) override {
    read_sizes.push_back(buffer.size());
    if (error && offset_ == bytes_.size()) {
      return IoResult<std::size_t>(*error);
    }
    const std::size_t size = std::min(buffer.size(), bytes_.size() - offset_);
    std::copy_n(bytes_.begin() + static_cast<std::ptrdiff_t>(offset_), size, buffer.begin());
    offset_ += size;
    return IoResult<std::size_t>(size);
  }

  void Release() noexcept override {
    released = true;
  }

  std::vector<std::size_t> read_sizes;
  std::optional<IoError> error;
  bool released = false;

private:
  Bytes bytes_;
  std::size_t offset_ = 0;
};

class MemoryOutputState final : public detail::OutputStreamState {
public:
  IoResult<void> Write(std::span<const std::byte> data) override {
    write_sizes.push_back(data.size());
    if (write_error) {
      return IoResult<void>(*write_error);
    }
    bytes.insert(bytes.end(), data.begin(), data.end());
    return IoResult<void>::Success();
  }

  IoResult<void> Close() override {
    ++close_count;
    if (close_error) {
      return IoResult<void>(*close_error);
    }
    return IoResult<void>::Success();
  }

  void Abort() noexcept override {
    aborted = true;
  }

  Bytes bytes;
  std::optional<IoError> write_error;
  std::optional<IoError> close_error;
  std::vector<std::size_t> write_sizes;
  std::size_t close_count = 0;
  bool aborted = false;
};

} // namespace

static_assert(std::move_constructible<InputStream>);
static_assert(!std::copy_constructible<InputStream>);
static_assert(std::move_constructible<OutputStream>);
static_assert(!std::copy_constructible<OutputStream>);
static_assert(std::move_constructible<AsyncInputStream>);
static_assert(!std::copy_constructible<AsyncInputStream>);
static_assert(std::move_constructible<AsyncOutputStream>);
static_assert(!std::copy_constructible<AsyncOutputStream>);

TEST_CASE("InputStreamUsesCallerProvidedReadCapacityAndCachesEof") {
  auto state =
      std::make_shared<MemoryInputState>(Bytes{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}, std::byte{5}});
  InputStream stream = detail::StreamAccess::MakeInputStream(state);
  Bytes buffer(3);

  REQUIRE(stream.Read(buffer).Value() == 3);
  REQUIRE(stream.Read(buffer).Value() == 2);
  REQUIRE(stream.Read(buffer).Value() == 0);
  REQUIRE(stream.Read(buffer).Value() == 0);
  REQUIRE(state->read_sizes == std::vector<std::size_t>{3, 3, 3});
  REQUIRE_THROWS_AS(stream.Read({}), std::invalid_argument);
}

TEST_CASE("InputStreamCopyToUsesCallerBufferAndLeavesOutputOpen") {
  auto input_state =
      std::make_shared<MemoryInputState>(Bytes{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}, std::byte{5}});
  auto output_state = std::make_shared<MemoryOutputState>();
  InputStream input = detail::StreamAccess::MakeInputStream(input_state);
  OutputStream output = detail::StreamAccess::MakeOutputStream(output_state);
  Bytes buffer(2);

  REQUIRE(input.CopyTo(output, buffer).Value() == 5);
  REQUIRE(output_state->bytes == Bytes{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}, std::byte{5}});
  REQUIRE(output_state->write_sizes == std::vector<std::size_t>{2, 2, 1});
  REQUIRE(output_state->close_count == 0);
  REQUIRE_FALSE(output_state->aborted);

  REQUIRE(output.Close().Succeeded());
  REQUIRE(output.Close().Succeeded());
  REQUIRE(output_state->close_count == 1);
  REQUIRE_THROWS_AS(output.Write({}), std::logic_error);
}

TEST_CASE("OutputStreamAbortsOnlyWhenDestroyedBeforeClose") {
  auto abandoned_state = std::make_shared<MemoryOutputState>();
  {
    OutputStream output = detail::StreamAccess::MakeOutputStream(abandoned_state);
    REQUIRE(output.Write({}).Succeeded());
  }
  REQUIRE(abandoned_state->aborted);

  auto closed_state = std::make_shared<MemoryOutputState>();
  {
    OutputStream output = detail::StreamAccess::MakeOutputStream(closed_state);
    REQUIRE(output.Close().Succeeded());
  }
  REQUIRE_FALSE(closed_state->aborted);
}

TEST_CASE("MovingStreamsTransfersReleaseOwnership") {
  auto input_state = std::make_shared<MemoryInputState>(Bytes{std::byte{1}});
  InputStream input = detail::StreamAccess::MakeInputStream(input_state);
  InputStream moved_input = std::move(input);
  Bytes buffer(1);
  REQUIRE_THROWS_AS(input.Read(buffer), std::logic_error);
  REQUIRE_FALSE(input_state->released);
  REQUIRE(moved_input.Read(buffer).Value() == 1);

  auto output_state = std::make_shared<MemoryOutputState>();
  OutputStream output = detail::StreamAccess::MakeOutputStream(output_state);
  OutputStream moved_output = std::move(output);
  REQUIRE_THROWS_AS(output.Write({}), std::logic_error);
  REQUIRE_FALSE(output_state->aborted);
  REQUIRE(moved_output.Close().Succeeded());
}

TEST_CASE("StreamCopyPreservesOperationalErrorsAndWrittenPrefixes") {
  auto input_state = std::make_shared<MemoryInputState>(Bytes{std::byte{1}, std::byte{2}});
  auto output_state = std::make_shared<MemoryOutputState>();
  InputStream input = detail::StreamAccess::MakeInputStream(input_state);
  OutputStream output = detail::StreamAccess::MakeOutputStream(output_state);
  Bytes buffer(1);
  const IoError error{IoErrorCode::PermissionDenied, "HuxerUI source access revoked"};
  input_state->error = error;

  const auto copied = input.CopyTo(output, buffer);
  REQUIRE_FALSE(copied.Succeeded());
  REQUIRE(copied.Error() == error);
  REQUIRE(output_state->bytes == Bytes{std::byte{1}, std::byte{2}});
  REQUIRE(output_state->close_count == 0);
  REQUIRE_THROWS_AS(input.Read(buffer), std::logic_error);
  REQUIRE(output.Close().Succeeded());
}

TEST_CASE("StreamOutputFailuresAreResultsAndDoNotFinalizeOutput") {
  const bool fail_close = GENERATE(false, true);
  const IoError error{IoErrorCode::NoSpace, "HuxerUI destination is full"};
  auto state = std::make_shared<MemoryOutputState>();
  {
    OutputStream output = detail::StreamAccess::MakeOutputStream(state);
    if (fail_close) {
      state->close_error = error;
      const auto result = output.Close();
      REQUIRE_FALSE(result.Succeeded());
      REQUIRE(result.Error() == error);
    } else {
      state->write_error = error;
      auto source = std::make_shared<MemoryInputState>(Bytes{std::byte{1}});
      InputStream input = detail::StreamAccess::MakeInputStream(source);
      Bytes buffer(1);
      const auto result = input.CopyTo(output, buffer);
      REQUIRE_FALSE(result.Succeeded());
      REQUIRE(result.Error() == error);
    }
    REQUIRE_THROWS_AS(output.Write({}), std::logic_error);
    REQUIRE_THROWS_AS(output.Close(), std::logic_error);
  }
  REQUIRE(state->aborted);
  REQUIRE(state->close_count == (fail_close ? 1 : 0));
}

TEST_CASE("IoErrorCategoriesPreservePortableSystemFailures") {
  REQUIRE(detail::IoErrorCategory(std::make_error_code(std::errc::timed_out)) == IoErrorCode::Timeout);
  REQUIRE(detail::IoErrorCategory(std::make_error_code(std::errc::no_space_on_device)) == IoErrorCode::NoSpace);
  REQUIRE(detail::IoErrorCategory(std::make_error_code(std::errc::permission_denied)) == IoErrorCode::PermissionDenied);
  REQUIRE(detail::IoErrorCategory(std::make_error_code(std::errc::io_error)) == IoErrorCode::Io);
}

} // namespace huxerui::test
