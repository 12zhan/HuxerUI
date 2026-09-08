#include "font_registry_internal.h"

#include <huxerui/data.h>
#include <huxerui/font.h>
#include <huxerui/resource.h>
#include <huxerui/text.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
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

// FNV-1a over the payload; collisions collapse distinct fonts onto one name,
// which 64 bits makes negligible for application font sets.
std::uint64_t HashBytes(const std::vector<std::byte>& bytes) {
  std::uint64_t hash = 14695981039346656037ULL;
  for (const std::byte byte : bytes) {
    hash ^= static_cast<std::uint64_t>(byte);
    hash *= 1099511628211ULL;
  }
  return hash;
}
}  // namespace

std::string InternFontBytes(std::vector<std::byte> bytes) {
  if (bytes.empty()) {
    throw std::invalid_argument("HuxerUI font payload must not be empty");
  }
  const std::string name = "huxerui-font-" + std::to_string(HashBytes(bytes));
  const std::scoped_lock lock(g_font_registry_mutex);
  g_font_registry.insert_or_assign(name, std::move(bytes));
  return name;
}

std::vector<std::byte> RegisteredFontData(std::string_view family) {
  const std::scoped_lock lock(g_font_registry_mutex);
  if (auto entry = g_font_registry.find(family); entry != g_font_registry.end()) {
    return entry->second;
  }
  return {};
}

}  // namespace huxerui::detail

namespace huxerui {

Font Font::FromRawAsset(const RawAsset& data, float size) {
  Bytes bytes;
  try {
    bytes = data.ReadBytes(true);
  } catch (const std::exception& exception) {
    throw std::invalid_argument(std::string("HuxerUI Font::FromRawAsset could not read font data: ") + exception.what());
  }
  if (bytes.empty()) {
    throw std::invalid_argument("HuxerUI Font::FromRawAsset font data must not be empty");
  }
  return Named(detail::InternFontBytes(std::move(bytes)), size);
}

Font Font::FromFile(std::string_view path, float size) {
  std::vector<std::byte> bytes = detail::ReadFileBytes(path);
  if (bytes.empty()) {
    throw std::invalid_argument("HuxerUI Font::FromFile could not read a non-empty font file");
  }
  return Named(detail::InternFontBytes(std::move(bytes)), size);
}

}  // namespace huxerui
