#include "BrailleRenderer.h"
#include <codecvt>
#include <locale>

BrailleRenderer::BrailleRenderer() {}
BrailleRenderer::~BrailleRenderer() {}

wchar_t BrailleRenderer::calculateBrailleChar(
    const std::array<uint8_t, 160 * 144> &frameBuffer, int startX, int startY) {
  // Braille dot mapping (Unicode U+2800 offset)
  // 0x01: Top-left
  // 0x02: Middle-left
  // 0x04: Bottom-left
  // 0x40: Bottom-bottom-left (row 4)
  // 0x08: Top-right
  // 0x10: Middle-right
  // 0x20: Bottom-right
  // 0x80: Bottom-bottom-right (row 4)
  static const int dotMap[4][2] = {
      {0x01, 0x08}, {0x02, 0x10}, {0x04, 0x20}, {0x40, 0x80}};

  int offset = 0;
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 2; ++x) {
      int px = startX + x;
      int py = startY + y;
      if (px < 160 && py < 144) {
        // Determine if pixel should be "on"
        // For now, anything other than 0 (white) is considered "on"
        if (frameBuffer[py * 160 + px] > 0) {
          offset |= dotMap[y][x];
        }
      }
    }
  }
  return static_cast<wchar_t>(0x2800 + offset);
}

std::string
BrailleRenderer::render(const std::array<uint8_t, 160 * 144> &frameBuffer) {
  std::wstring result;
  // 144 / 4 = 36 rows
  // 160 / 2 = 80 cols
  for (int y = 0; y < 144; y += 4) {
    for (int x = 0; x < 160; x += 2) {
      result += calculateBrailleChar(frameBuffer, x, y);
    }
    if (y < 144 - 4) {
      result += L'\n';
    }
  }

  // Convert wstring to string
  std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
  return converter.to_bytes(result);
}
