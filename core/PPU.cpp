#include "PPU.h"

PPU::PPU(Bus &b) : bus(b) { frameBuffer.fill(0); }

PPU::~PPU() {}

void PPU::tick() {
  // A scanline takes 456 T-cycles.
  // In a full implementation, we subtract the amount of T-cycles elapsed in the
  // CPU. Assuming this tick runs per CPU cycle for now:
  scanlineCounter--;

  if (scanlineCounter <= 0) {
    scanlineCounter = 456;
    currentScanline++;

    if (currentScanline == 144) {
      // V-Blank period starts
      // Frame is perfectly drawn now
      frameReady = true;
    } else if (currentScanline > 153) {
      // Reset scanline
      currentScanline = 0;
    }

    // Write current scanline to LY register (0xFF44)
    bus.write(0xFF44, currentScanline);
  }
}

uint8_t PPU::readOAM(uint16_t address) const { return oam[address - 0xFE00]; }

void PPU::writeOAM(uint16_t address, uint8_t value) {
  oam[address - 0xFE00] = value;
}

uint8_t PPU::read(uint16_t address) const { return vram[address - 0x8000]; }

void PPU::write(uint16_t address, uint8_t value) {
  vram[address - 0x8000] = value;
}

uint8_t PPU::readReg(uint16_t address) const {
  switch (address) {
  case 0xFF40:
    return lcdc;
  case 0xFF41:
    return stat;
  case 0xFF42:
    return scy;
  case 0xFF43:
    return scx;
  case 0xFF44:
    return currentScanline;
  case 0xFF45:
    return lyc;
  case 0xFF47:
    return bgp;
  case 0xFF48:
    return obp0;
  case 0xFF49:
    return obp1;
  case 0xFF4A:
    return wy;
  case 0xFF4B:
    return wx;
  default:
    return 0xFF; // Unmapped PPU registers return 0xFF
  }
}

void PPU::writeReg(uint16_t address, uint8_t value) {
  switch (address) {
  case 0xFF40:
    lcdc = value;
    break;
  case 0xFF41:
    stat = value;
    break;
  case 0xFF42:
    scy = value;
    break;
  case 0xFF43:
    scx = value;
    break;
  case 0xFF45:
    lyc = value;
    break;
  case 0xFF47:
    bgp = value;
    break;
  case 0xFF48:
    obp0 = value;
    break;
  case 0xFF49:
    obp1 = value;
    break;
  case 0xFF4A:
    wy = value;
    break;
  case 0xFF4B:
    wx = value;
    break;
  }
}
