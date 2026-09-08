#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace huxerui::detail {

/// Registers payload bytes under a stable generated family name and returns
/// it. Identical payload bytes share one registration and one name; the
/// registry is process-wide and lives until shutdown.
std::string InternFontBytes(std::vector<std::byte> bytes);

/// Returns the payload registered under family, or an empty buffer when the
/// name is not registered.
std::vector<std::byte> RegisteredFontData(std::string_view family);

}  // namespace huxerui::detail
