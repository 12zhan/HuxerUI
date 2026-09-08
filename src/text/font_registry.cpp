#include <huxerui/font.h>
#include <huxerui/data.h>
#include <huxerui/resource.h>

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace huxerui::detail {

namespace {
// Process-wide registry. Font payloads are small (usually well under a
// megabyte), so storing bytes keeps every renderer query free of filesystem
// and resource-service dependencies.
std::mutex g_font_registry_mutex;
std::map<std::string, std::vector<std::byte>, std::less<>> g_font_registry;

std::vector<std::byte> ReadFileBytes(std::string_view path) {
  std::ifstream file(std::string(path), std::ios::binary);
  if (!file) {
    return {};
  }
  file.seekg(0, std::ios::end);
  const std::streamoff size = file.tellg();
  if (size <= 0) {
    return {};
  }
  file.seekg(0, std::ios::beg);
  std::vector<std::byte> bytes(static_cast<std::size_t>(size));
  file.read(reinterpret_cast<char*>(bytes.data()), size);
  if (file.gcount() != size) {
    return {};
  }
  return bytes;
}
}  // namespace

std::vector<std::byte> RegisteredFontData(std::string_view family) {
  const std::scoped_lock lock(g_font_registry_mutex);
  if (auto entry = g_font_registry.find(family); entry != g_font_registry.end()) {
    return entry->second;
  }
  return {};
}

}  // namespace huxerui::detail

namespace huxerui {

bool RegisterFont(std::string_view family, const RawAsset& data) {
  if (family.empty()) {
    throw std::invalid_argument("HuxerUI RegisterFont family must not be empty");
  }
  Bytes bytes;
  try {
    bytes = data.ReadBytes(true);
  } catch (const std::exception&) {
    return false;
  }
  if (bytes.empty()) {
    return false;
  }
  const std::scoped_lock lock(detail::g_font_registry_mutex);
  detail::g_font_registry.insert_or_assign(std::string(family), std::move(bytes));
  return true;
}

bool RegisterFont(std::string_view family, std::string_view path) {
  if (family.empty()) {
    throw std::invalid_argument("HuxerUI RegisterFont family must not be empty");
  }
  if (path.empty()) {
    throw std::invalid_argument("HuxerUI RegisterFont path must not be empty");
  }
  std::vector<std::byte> bytes = detail::ReadFileBytes(path);
  if (bytes.empty()) {
    return false;
  }
  const std::scoped_lock lock(detail::g_font_registry_mutex);
  detail::g_font_registry.insert_or_assign(std::string(family), std::move(bytes));
  return true;
}

}  // namespace huxerui
