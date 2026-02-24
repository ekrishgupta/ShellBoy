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

  renderSprites();
}

void PPU::renderSprites() {
  if ((lcdc & 0x02) == 0)
    return; // Sprites disabled

  bool use8x16 = (lcdc & 0x04) != 0;

  // Game Boy can render up to 40 sprites, but only 10 per scanline.
  // On DMG, priority is determined by X-coordinate (lower X = higher priority)
  // then OAM index. For simplicity, we loop through all and enforce the
  // 10-limit. To handle overlapping sprites correctly (earlier OAM index =
  // higher priority), we loop backwards so earlier ones overwrite.

  int spritesOnLine = 0;
  // First pass: find sprites on this line (up to 10)
  struct Sprite {
    uint8_t y, x, tile, attr;
  };
  std::vector<Sprite> spritesToRender;

  for (int i = 0; i < 40; i++) {
    uint8_t y = oam[i * 4] - 16;
    uint8_t x = oam[i * 4 + 1] - 8;
    uint8_t height = use8x16 ? 16 : 8;

    if (currentScanline >= y && currentScanline < (y + height)) {
      if (spritesToRender.size() < 10) {
        spritesToRender.push_back(
            {oam[i * 4], oam[i * 4 + 1], oam[i * 4 + 2], oam[i * 4 + 3]});
      }
    }
  }

  // Render the sprites we found in reverse order of OAM index
  for (int i = (int)spritesToRender.size() - 1; i >= 0; i--) {
    const auto &s = spritesToRender[i];
    int yPos = s.y - 16;
    int xPos = s.x - 8;
    uint8_t tileIndex = s.tile;
    uint8_t attr = s.attr;

    bool yFlip = (attr & 0x40) != 0;
    bool xFlip = (attr & 0x20) != 0;
    bool priority = (attr & 0x80) != 0; // 1: Behind BG color 1-3, 0: Above BG
    uint8_t paletteReg = (attr & 0x10) ? obp1 : obp0;

    int line = currentScanline - yPos;
    int height = use8x16 ? 16 : 8;

    if (yFlip) {
      line = height - 1 - line;
    }

    // In 8x16 mode, bit 0 of tile index is ignored.
    // The top tile is tileIndex & 0xFE, bottom is tileIndex | 0x01.
    uint16_t tileAddr;
    if (use8x16) {
      tileAddr = 0x8000 + ((tileIndex & 0xFE) * 16) + (line * 2);
    } else {
      tileAddr = 0x8000 + (tileIndex * 16) + (line * 2);
    }

    uint8_t data1 = read(tileAddr);
    uint8_t data2 = read(tileAddr + 1);

    for (int tilePixel = 0; tilePixel < 8; tilePixel++) {
      int colorBit = 7 - tilePixel;
      if (xFlip) {
        colorBit = tilePixel;
      }

      uint8_t colorNum = ((data2 >> colorBit) & 1) << 1;
      colorNum |= ((data1 >> colorBit) & 1);

      if (colorNum == 0)
        continue; // Color 0 is transparent

      int canvasX = xPos + tilePixel;
      if (canvasX < 0 || canvasX >= 160)
        continue;

      // Priority check: BGP color 0 is always behind sprites.
      // If priority bit is set, sprite is behind BG color 1-3.
      if (priority && frameBuffer[currentScanline * 160 + canvasX] != 0) {
        continue;
      }

      uint8_t color = (paletteReg >> (colorNum * 2)) & 3;
      frameBuffer[currentScanline * 160 + canvasX] = color;
    }
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
