#include "font_registry_internal.h"

#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace huxerui::detail {

namespace {
// Private family-name transport used only by the Android JNI pull; see the contract in the internal
// header. Records hold weak handles, so no payload bytes are retained here and stale names are
// dropped as soon as they are observed.
std::mutex g_font_payload_mutex;
std::map<std::string, std::weak_ptr<const FontData>, std::less<>> g_font_payloads;
}  // namespace

void TrackFontPayload(const std::shared_ptr<const FontData>& payload) {
  if (payload == nullptr || payload->family.empty()) {
    throw std::logic_error("HuxerUI font payload tracking requires a named payload");
  }
  const std::scoped_lock lock(g_font_payload_mutex);
  std::erase_if(g_font_payloads, [](const auto& entry) { return entry.second.expired(); });
  g_font_payloads.insert_or_assign(payload->family, std::weak_ptr<const FontData>(payload));
}

std::shared_ptr<const FontData> FindFontPayload(std::string_view family) {
  const std::scoped_lock lock(g_font_payload_mutex);
  if (auto entry = g_font_payloads.find(family); entry != g_font_payloads.end()) {
    if (auto payload = entry->second.lock()) {
      return payload;
    }
    g_font_payloads.erase(entry);
  }
  return nullptr;
}

}  // namespace huxerui::detail
