#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include <huxerui/data.h>
#include <huxerui/stream.h>
#include <huxerui/task.h>

namespace huxerui {

namespace detail {
class HttpOperationState;
class HttpTransport;
} // namespace detail

/// HTTP request methods supported by HttpClient.
enum class HttpMethod {
  Get,     ///< Retrieves a representation without a request body.
  Head,    ///< Retrieves response metadata without a response body.
  Post,    ///< Submits the request body to the target resource.
  Put,     ///< Replaces the target resource with the request body.
  Patch,   ///< Applies a partial update from the request body.
  Delete,  ///< Requests deletion of the target resource.
  Options, ///< Requests the communication options for the target resource.
};

/// One HTTP header field. Names compare case-sensitively as stored even though HTTP field-name matching is
/// case-insensitive.
struct HttpHeader {
  /// The field name without a trailing colon. HttpClient rejects empty names and invalid HTTP token characters.
  std::string name;

  /// The field value. HttpClient rejects embedded null, carriage-return, and line-feed characters.
  std::string value;

  /// Compares the stored name and value exactly.
  bool operator==(const HttpHeader&) const = default;
};

/// A complete, owned HTTP request declaration.
///
/// HttpClient validates the declaration synchronously before returning a Task. The URL must be absolute and use HTTP
/// or HTTPS, GET and HEAD requests must not contain a body, and a specified timeout must be positive.
struct HttpRequest {
  /// The absolute HTTP or HTTPS URL. Redirects may cause HttpResponse::url to differ from this value.
  std::string url;

  /// The request method. GET is used by default.
  HttpMethod method = HttpMethod::Get;

  /// Request headers in declaration order. Repeated field names are preserved.
  std::vector<HttpHeader> headers{};

  /// The owned binary request body. Text and structured formats must be encoded by application code.
  Bytes body{};

  /// The deadline for the complete operation, including redirects and streamed body reads. std::nullopt disables the
  /// HuxerUI deadline but cannot disable platform- or network-imposed failures.
  std::optional<std::chrono::milliseconds> timeout = std::chrono::milliseconds{30000};
};

/// A fully buffered final HTTP response returned by HttpClient::SendAsync().
///
/// HTTP status codes such as 404 and 500 are valid responses. Only transport, timeout, or unsupported-capability
/// failures produce HttpError instead.
struct HttpResponse {
  /// The final URL after platform-managed redirects.
  std::string url;

  /// The HTTP status code reported by the final response.
  int status_code = 0;

  /// Final response headers. Repeated fields remain separate when the platform exposes them separately.
  std::vector<HttpHeader> headers;

  /// The complete binary response body after any decoding performed by the platform transport.
  Bytes body;

  /// Compares all stored response fields exactly.
  bool operator==(const HttpResponse&) const = default;
};

/// Categories of failures that prevent an HTTP operation from producing its requested result.
enum class HttpErrorCode {
  Transport,  ///< URL loading, TLS, connection, protocol, or platform transport failure.
  Timeout,    ///< The request deadline elapsed before the complete response body was consumed.
  Unsupported ///< The current platform adapter cannot provide the requested HTTP capability.
};

/// A structured HTTP operation failure.
struct HttpError {
  /// The stable error category for application decisions.
  HttpErrorCode code;

  /// An English diagnostic intended for logging or user-facing adaptation by the application.
  std::string message;

  /// Compares the error code and message exactly.
  bool operator==(const HttpError&) const = default;
};

/// An HTTP response or operation error, using the shared Result contract.
/// Succeeded() means a response is available through Value(), including HTTP 4xx and 5xx statuses.
/// SendStreamAsync() reports pre-header failures here; later body operations return IoResult.
template <class T> using HttpResult = Result<T, HttpError>;

/// The transfer direction described by HttpProgress.
enum class HttpProgressKind {
  Upload,   ///< Request-body bytes handed to or accepted by the platform transport.
  Download, ///< Response-body bytes delivered to application code.
};

/// One monotonic transfer-progress observation.
///
/// Upload progress does not mean that the server acknowledged the bytes. Download progress counts bytes returned by
/// HttpResponseStream::Body() or accumulated by HttpClient::SendAsync() after any platform decoding. Throwing from the
/// callback cancels the request and rethrows the exception from the current Task.
struct HttpProgress {
  /// The transfer direction for this observation.
  HttpProgressKind kind;

  /// Bytes transferred for this direction. Values never decrease within one logical request.
  std::uint64_t transferred_bytes = 0;

  /// A reliable total for the same delivered representation when known. Encoded, chunked, or otherwise ambiguous
  /// responses may omit it.
  std::optional<std::uint64_t> total_bytes;

  /// Compares all progress fields exactly.
  bool operator==(const HttpProgress&) const = default;
};

/// A move-only, pull-based final HTTP response body.
///
/// Final URL, status, and headers are available immediately after HttpClient::SendStreamAsync() succeeds. Body()
/// exposes the shared AsyncInputStream contract: the caller chooses every maximum read size, empty Bytes means EOF,
/// and post-header transport failures return IoError. Destroying an unfinished response cancels the operation.
///
/// Example:
/// @code
/// Task<void> ConsumeResponse(HttpResponseStream stream) {
///   while (true) {
///     auto result = co_await stream.Body().ReadAsync(32 * 1024);
///     if (!result.Succeeded()) {
///       ReportIoError(result.Error());
///       co_return;
///     }
///     Bytes data = std::move(result).Value();
///     if (data.empty()) {
///       co_return;
///     }
///     ConsumeChunk(std::move(data));
///   }
/// }
/// @endcode
class HttpResponseStream final {
public:
  /// A response stream has unique ownership and cannot be copied.
  HttpResponseStream(const HttpResponseStream&) = delete;
  HttpResponseStream& operator=(const HttpResponseStream&) = delete;

  /// Transfers ownership of the response operation. Accessing other after the move throws std::logic_error.
  HttpResponseStream(HttpResponseStream&& other) noexcept;

  /// Cancels any unfinished operation currently owned by this object, then transfers ownership from other.
  HttpResponseStream& operator=(HttpResponseStream&& other) noexcept;

  /// Releases the owned body stream, canceling the operation unless EOF or an error has already been consumed.
  ~HttpResponseStream();

  /// Returns the final URL after redirects. Throws std::logic_error when this stream has been moved from.
  [[nodiscard]] const std::string& Url() const;

  /// Returns the final HTTP status code. Throws std::logic_error when this stream has been moved from.
  [[nodiscard]] int StatusCode() const;

  /// Returns immutable final response headers whose lifetime is tied to this response operation. Throws
  /// std::logic_error when this stream has been moved from.
  [[nodiscard]] std::span<const HttpHeader> Headers() const;

  /// Returns the uniquely owned response body stream. The reference remains valid until this response is moved or
  /// destroyed. Accessing a moved-from response throws std::logic_error.
  [[nodiscard]] AsyncInputStream& Body();

private:
  explicit HttpResponseStream(std::shared_ptr<detail::HttpOperationState> state);

  std::shared_ptr<detail::HttpOperationState> state_;
  AsyncInputStream body_;

  friend class detail::HttpOperationState;
};

/// The per-window HTTP service backed by the current platform networking stack.
///
/// Obtain the shared service with UseService<HttpClient>() during composition, capture it into a Task launched from
/// UseTaskScope(), and call SendAsync() or SendStreamAsync() directly. HTTP already owns its platform-appropriate
/// asynchronous path, so it must not be wrapped in RunWorker(). Task continuations and progress callbacks run on the
/// owning Runtime UI thread and may update captured State directly without TaskScope::Post().
///
/// Example:
/// @code
/// Task<void> LoadProfile(const std::shared_ptr<HttpClient>& http) {
///   HttpResult<HttpResponse> result = co_await http->SendAsync(
///       {.url = "https://api.example.com/profile"},
///       [](HttpProgress progress) {
///         if (progress.kind == HttpProgressKind::Download) {
///           UpdateDownloadProgress(progress.transferred_bytes, progress.total_bytes);
///         }
///       });
///   if (!result.Succeeded()) {
///     ReportHttpError(result.Error());
///     co_return;
///   }
///   HttpResponse response = std::move(result).Value();
///   UseProfileBytes(std::move(response.body));
/// }
/// @endcode
class HttpClient final {
public:
  /// Destroys the service handle. In-flight operations retain the platform transport until they finish or are canceled.
  ~HttpClient();

  /// HttpClient is a Runtime-owned service handle and cannot be copied or moved.
  HttpClient(const HttpClient&) = delete;
  HttpClient& operator=(const HttpClient&) = delete;
  HttpClient(HttpClient&&) = delete;
  HttpClient& operator=(HttpClient&&) = delete;

  /// Sends request and buffers the complete final response body. progress is optional and observes upload and download
  /// transfer on the owning Runtime UI thread. Invalid request configuration throws std::invalid_argument
  /// synchronously; platform failures are returned through HttpResult<HttpResponse>.
  [[nodiscard]] Task<HttpResult<HttpResponse>> SendAsync(HttpRequest request, std::function<void(HttpProgress)> progress = {}) const;

  /// Sends request and returns after final response headers are available. progress is optional and observes upload and
  /// consumed download bytes on the owning Runtime UI thread. Invalid request configuration throws
  /// std::invalid_argument synchronously; pre-header platform failures are returned through
  /// HttpResult<HttpResponseStream>. Later body operations return IoResult.
  ///
  /// Example:
  /// @code
  /// HttpResult<HttpResponseStream> opened = co_await http->SendStreamAsync({.url = "https://api.example.com/archive"});
  /// if (!opened.Succeeded()) {
  ///   ReportHttpError(opened.Error());
  ///   co_return;
  /// }
  /// co_await ConsumeResponse(std::move(opened).Value());
  /// @endcode
  [[nodiscard]] Task<HttpResult<HttpResponseStream>> SendStreamAsync(HttpRequest request, std::function<void(HttpProgress)> progress = {}) const;

private:
  explicit HttpClient(std::shared_ptr<detail::HttpTransport> transport);

  std::shared_ptr<detail::HttpTransport> transport_;

  friend class Runtime;
};

} // namespace huxerui
