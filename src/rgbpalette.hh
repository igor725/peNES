#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

class PaletteRGB {
  static constexpr double PI_NUM = 3.14159265358979323846;

  public:
  PaletteRGB() { updatePalette(); }

  void setHueShift(float rad) {
    m_hueShift = rad;
    updatePalette();
  }

  void setSaturation(float sat) {
    m_saturation = sat;
    updatePalette();
  }

  void setContrast(float cst) {
    m_contrast = cst;
    updatePalette();
  }

  uint32_t operator[](uint8_t index) const { return m_palette[index & 0b111111]; }

  protected:
  void updatePalette() {
    const float lumaLow[4]  = {-0.117f, 0.000f, 0.308f, 0.715f};
    const float lumaHigh[4] = {0.397f, 0.681f, 1.000f, 1.000f};

    for (uint8_t i = 0; i < 64; ++i) {
      uint8_t color = i & 0x0F, level = (i >> 4) & 0x03;

      float v0, v1;
      if (color == 0x00) {
        v0 = v1 = lumaHigh[level];
      } else if (color == 0x0D) {
        v0 = v1 = lumaLow[1];
      } else if (color > 0x0D) {
        v0 = v1 = lumaLow[level];
      } else {
        v0 = lumaLow[level];
        v1 = lumaHigh[level];
      }

      float rSum = 0.0f, gSum = 0.0f, bSum = 0.0f;
      for (uint8_t p = 0; p < 12; ++p) {
        float sig;
        if (color != 0 && color < 0x0D) {
          sig = (((p + color + 8) % 12) < 6) ? v1 : v0;
        } else {
          sig = v0;
        }

        float angle = p * (PI_NUM / 6.0f) + m_hueShift;
        float y     = sig;
        float iComp = sig * std::cos(angle) * m_saturation;
        float qComp = sig * std::sin(angle) * m_saturation;

        rSum += y + 0.956f * iComp + 0.621f * qComp * m_contrast;
        gSum += y - 0.272f * iComp - 0.647f * qComp * m_contrast;
        bSum += y - 1.106f * iComp + 1.703f * qComp * m_contrast;
      }

      float r = std::pow(std::max(0.0f, rSum / 12.0f), 1.0f) * 255.f;
      float g = std::pow(std::max(0.0f, gSum / 12.0f), 1.0f) * 255.f;
      float b = std::pow(std::max(0.0f, bSum / 12.0f), 1.0f) * 255.f;

      m_palette[i] = 0xFF000000 | (static_cast<uint8_t>(std::clamp(r, 0.0f, 255.0f)) << 16) | (static_cast<uint8_t>(std::clamp(g, 0.0f, 255.0f)) << 8) |
                     static_cast<uint8_t>(std::clamp(b, 0.0f, 255.0f));
    }
  }

  private:
  float m_hueShift   = 0.0f;
  float m_saturation = 1.0f;
  float m_contrast   = 1.0f;

  uint32_t m_palette[64];
};
