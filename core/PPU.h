#pragma once

#include "Bus.h"
#include <array>
#include <cstdint>
#include <vector>

class PPU {
public:
  explicit PPU(Bus &bus);
  ~PPU();

  void tick();

  enum class Mode : uint8_t {
    HBlank = 0,
    VBlank = 1,
    OAMSearch = 2,
    PixelTransfer = 3
  };

  Mode getMode() const { return static_cast<Mode>(stat & 0x03); }
  void setMode(Mode mode);
  void updateStatus();

  uint8_t read(uint16_t address) const;
  void write(uint16_t address, uint8_t value);

  uint8_t readOAM(uint16_t address) const;
  void writeOAM(uint16_t address, uint8_t value);

  uint8_t readReg(uint16_t address) const;
  void writeReg(uint16_t address, uint8_t value);

  // The display is logically 160x144 pixels.
  // 0 = white, 1 = light gray, 2 = dark gray, 3 = black
  std::array<uint8_t, 160 * 144> frameBuffer{};
  bool frameReady = false;

  enum class Mode { HBlank = 0, VBlank = 1, OAMSearch = 2, PixelTransfer = 3 };

  Mode getMode() const { return static_cast<Mode>(stat & 0x03); }

private:
  Bus &bus;
  int scanlineCounter = 456; // T-cycles per scanline
  uint8_t currentScanline = 0;

  void renderScanline();
  void renderSprites();
  void setMode(Mode mode);
  void updateStatus();

  std::array<uint8_t, 0x2000> vram{};
  std::array<uint8_t, 0xA0> oam{};

  uint8_t lcdc = 0;
  uint8_t stat = 0;
  uint8_t scy = 0;
  uint8_t scx = 0;
  uint8_t lyc = 0;
  uint8_t bgp = 0;
  uint8_t obp0 = 0;
  uint8_t obp1 = 0;
  uint8_t wy = 0;
  uint8_t wx = 0;
};
