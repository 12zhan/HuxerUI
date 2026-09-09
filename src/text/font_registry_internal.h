#pragma once

#include <memory>
#include <string_view>

#include <huxerui/text.h>

namespace huxerui::detail {

/// Family-name transport for hosts whose renderer resolves custom fonts outside C++: the Android
/// renderer passes generated family names to Java, and Java pulls the matching bytes through a JNI
/// export. This is not a font cache; entries hold weak handles, so payload bytes stay owned by live
/// Font values and are reclaimed with them. Every other renderer reads the payload directly from the
/// Font value through InternalAccess::FontPayload and never consults this table.
void TrackFontPayload(const std::shared_ptr<const FontData>& payload);

/// Returns the live payload recorded under family, or nullptr when the name is unknown or its
/// payload was reclaimed together with the last Font values referencing it.
[[nodiscard]] std::shared_ptr<const FontData> FindFontPayload(std::string_view family);

}  // namespace huxerui::detail
