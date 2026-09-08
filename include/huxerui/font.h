#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

namespace huxerui {

namespace detail {

/// Returns the registered payload for family, or an empty buffer when the
/// family is unregistered. `Font::FromRawAsset` and `Font::FromFile` register
/// payloads under stable generated family names, and platform renderers call
/// this before their system font lookup; the definition lives in the shared
/// core.
std::vector<std::byte> RegisteredFontData(std::string_view family);

}  // namespace detail

}  // namespace huxerui
