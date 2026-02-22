#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

class BrailleRenderer {
public:
  BrailleRenderer();
  ~BrailleRenderer();

  // Renders the 160x144 Game Boy frame buffer into an 80x36 Unicode Braille
  // string
  std::string render(const std::array<uint8_t, 160 * 144> &frameBuffer);

private:
  // Helper to calculate the Braille character from a 2x4 pixel grid.
  // 1 in the grid = pixel is drawn.
  wchar_t
  calculateBrailleChar(const std::array<uint8_t, 160 * 144> &frameBuffer,
                       int startX, int startY);
};
