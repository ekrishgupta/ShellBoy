#include "PPU.h"

PPU::PPU(Bus &b) : bus(b) { frameBuffer.fill(0); }

PPU::~PPU() {}

void PPU::tick() {
  // A scanline takes 456 T-cycles.
  scanlineCounter--;

  if (scanlineCounter <= 0) {
    scanlineCounter = 456;
    currentScanline++;

    if (currentScanline < 144) {
      renderScanline();
    }

    if (currentScanline == 144) {
      // V-Blank period starts
      stat = (stat & 0xFC) | 1; // Mode 1
      bus.requestInterrupt(Bus::INTERRUPT_VBLANK);
      frameReady = true;
    } else if (currentScanline > 153) {
      // Reset scanline
      currentScanline = 0;
      stat = (stat & 0xFC) | 2; // Mode 2
    } else if (currentScanline < 144) {
      stat = (stat & 0xFC) | 2; // Mode 2 (approximation for now)
    }

    // Write current scanline to LY register (0xFF44)
    bus.write(0xFF44, currentScanline);

    // Update LYC=LY Coincidence flag (Bit 2 of STAT)
    if (currentScanline == lyc) {
      stat |= 0x04;
      // Note: LYC interrupt should also be triggered here if enabled in STAT
    } else {
      stat &= ~0x04;
    }
  }
}

void PPU::renderScanline() {
  // LCD Enable check
  if ((lcdc & 0x80) == 0)
    return;

  // Background Enable check
  if ((lcdc & 0x01) == 0)
    return;

  uint16_t tileMap = (lcdc & 0x08) ? 0x9C00 : 0x9800;
  uint16_t tileData = (lcdc & 0x10) ? 0x8000 : 0x8800;
  bool unsig = (lcdc & 0x10) != 0;

  uint8_t yPos = currentScanline + scy;
  uint16_t tileRow = (yPos / 8) * 32;

  for (int pixel = 0; pixel < 160; ++pixel) {
    uint8_t xPos = pixel + scx;
    uint16_t tileCol = xPos / 8;

    uint16_t tileAddr = tileMap + tileRow + tileCol;
    int16_t tileNum;
    if (unsig) {
      tileNum = read(tileAddr);
    } else {
      tileNum = static_cast<int8_t>(read(tileAddr));
    }

    uint16_t tileLocation = tileData;
    if (unsig) {
      tileLocation += (tileNum * 16);
    } else {
      tileLocation += ((tileNum + 128) * 16);
    }

    uint8_t line = yPos % 8;
    uint8_t data1 = read(tileLocation + (line * 2));
    uint8_t data2 = read(tileLocation + (line * 2) + 1);

    int colorBit = xPos % 8;
    colorBit -= 7;
    colorBit *= -1;

    uint8_t colorNum = ((data2 >> colorBit) & 1) << 1;
    colorNum |= ((data1 >> colorBit) & 1);

    // Map color through BGP palette
    uint8_t color = (bgp >> (colorNum * 2)) & 3;

    int bufferOffset = (currentScanline * 160) + pixel;
    frameBuffer[bufferOffset] = color;
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
