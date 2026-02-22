#pragma once

#include "Bus.h"
#include <array>
#include <cstdint>

class PPU {
public:
  explicit PPU(Bus &bus);
  ~PPU();

  void tick();

  // The display is logically 160x144 pixels.
  // 0 = white, 1 = light gray, 2 = dark gray, 3 = black
  std::array<uint8_t, 160 * 144> frameBuffer{};
  bool frameReady = false;

private:
  Bus &bus;
  int scanlineCounter = 456; // T-cycles per scanline
  uint8_t currentScanline = 0;
};
