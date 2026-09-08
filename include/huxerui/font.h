#pragma once

#include <huxerui/resource.h>

#include <cstddef>
#include <string_view>
#include <vector>

namespace huxerui {

/// Registers font file bytes (ttf/otf) as a named family for the current
/// process. Later `Font::Named(family, ...)` resolves to the registered font
/// on every platform renderer before the system family table, keeping text
/// measurement and painting consistent. Registration is process-wide and
/// lives until shutdown; re-registering a family replaces its data.
///
/// Data-backed assets are read immediately, so the registration keeps working
/// after the Runtime shuts down.
///
/// @param family the family name `Font::Named` matches.
/// @param data font file payload.
/// @return false when the payload cannot be read or is empty. Throws
/// `std::invalid_argument` for an empty family name.
[[nodiscard]] bool RegisterFont(std::string_view family, const RawAsset& data);

/// Registers a font file already present on the local filesystem. The file is
/// read during registration, so it does not need to outlive the call.
///
/// @param family the family name `Font::Named` matches.
/// @param path readable font file path.
/// @return false when the file cannot be read or is empty. Throws
/// `std::invalid_argument` for an empty family name or path.
[[nodiscard]] bool RegisterFont(std::string_view family, std::string_view path);

namespace detail {

/// Returns the registered payload for family, or an empty buffer when the
/// family is unregistered. Platform renderers call this before their system
/// font lookup; the definition lives in the shared core.
std::vector<std::byte> RegisteredFontData(std::string_view family);

}  // namespace detail

}  // namespace huxerui
