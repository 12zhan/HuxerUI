#pragma once

#include <cstdint>

namespace huxerui {

struct Color {
  float red = 0.0F;
  float green = 0.0F;
  float blue = 0.0F;
  float alpha = 1.0F;

  bool operator==(const Color&) const = default;

  static constexpr Color Rgb(int red, int green, int blue, float alpha = 1.0F) noexcept {
    return {
        static_cast<float>(red) / 255.0F,
        static_cast<float>(green) / 255.0F,
        static_cast<float>(blue) / 255.0F,
        alpha,
    };
  }

  /// Decodes packed channels in 0xRRGGBBAA byte order.
  static constexpr Color Rgba32(std::uint32_t rgba) noexcept {
    return {
        static_cast<float>((rgba >> 24U) & 0xFFU) / 255.0F,
        static_cast<float>((rgba >> 16U) & 0xFFU) / 255.0F,
        static_cast<float>((rgba >> 8U) & 0xFFU) / 255.0F,
        static_cast<float>(rgba & 0xFFU) / 255.0F,
    };
  }

  /// Decodes packed channels in 0xAARRGGBB byte order.
  static constexpr Color Argb32(std::uint32_t argb) noexcept {
    return {
        static_cast<float>((argb >> 16U) & 0xFFU) / 255.0F,
        static_cast<float>((argb >> 8U) & 0xFFU) / 255.0F,
        static_cast<float>(argb & 0xFFU) / 255.0F,
        static_cast<float>((argb >> 24U) & 0xFFU) / 255.0F,
    };
  }

  static constexpr Color Transparent() noexcept {
    return {0.0F, 0.0F, 0.0F, 0.0F};
  }

  static constexpr Color Black() noexcept {
    return {0.0F, 0.0F, 0.0F, 1.0F};
  }

  static constexpr Color White() noexcept {
    return {1.0F, 1.0F, 1.0F, 1.0F};
  }
};

} // namespace huxerui
