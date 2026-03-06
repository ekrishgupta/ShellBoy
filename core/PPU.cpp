#include "PPU.h"

PPU::PPU(Bus &b) : bus(b) { frameBuffer.fill(0); lcdc = 0x91; stat = 0x85; bgp = 0xFC; obp0 = 0xFF; obp1 = 0xFF; }

PPU::~PPU() {}

void PPU::reset() {
  lcdc = 0x91;
  stat = 0x85;
  bgp = 0xFC;
  obp0 = 0xFF;
  obp1 = 0xFF;
  scy = scx = lyc = wy = wx = 0;
  scanlineCounter = 456;
  currentScanline = 0;
  vramBank = 0;
  windowLineCounter = 0;
}

void PPU::tick() {
  if (!(lcdc & 0x80)) {
    // LCD disabled: reset variables and stay in Mode 0 (or 2?)
    scanlineCounter = 456;
    currentScanline = 0;
    stat &= ~0x03; // Clear mode
    return;
  }

  scanlineCounter--;

  if (scanlineCounter <= 0) {
    scanlineCounter = 456;
    currentScanline++;
    updateStatus();

    if (currentScanline > 153) {
      currentScanline = 0;
      windowLineCounter = 0;
      updateStatus();
    }
  }

  // Mode transitions
  if (currentScanline >= 144) {
    if (getMode() != Mode::VBlank) {
      setMode(Mode::VBlank);
    }
  } else {
    if (scanlineCounter > 456 - 80) { // Mode 2: OAM Search (80 dots)
      if (getMode() != Mode::OAMSearch) {
        setMode(Mode::OAMSearch);
        // Game Boy hardware quirk: Mode 3 duration varies with sprite count.
        // Penalty is roughly 6-10 cycles per sprite.
        int spritesOnLine = 0;
        uint8_t spriteHeight = (lcdc & 0x04) ? 16 : 8;
        for (int i = 0; i < 40; i++) {
          uint8_t y = oam[i * 4];
          if (currentScanline + 16 >= y &&
              currentScanline + 16 < (y + spriteHeight)) {
            spritesOnLine++;
            if (spritesOnLine >= 10)
              break;
          }
        }
        mode3Duration = 172 + (spritesOnLine * 10) + (scx % 8);
      }
    } else if (scanlineCounter >
               456 - 80 - mode3Duration) { // Mode 3: Pixel Transfer
      if (getMode() != Mode::PixelTransfer) {
        setMode(Mode::PixelTransfer);
      }
    } else { // Mode 0: H-Blank
      if (getMode() != Mode::HBlank) {
        setMode(Mode::HBlank);
        renderScanline(); // Render when H-Blank starts
      }
    }
  }
}

void PPU::setMode(Mode mode) {
  stat = (stat & 0xFC) | static_cast<uint8_t>(mode);

  bool interrupt = false;
  switch (mode) {
  case Mode::HBlank:
    interrupt = (stat & 0x08);
    bus.processHDMA();
    break;
  case Mode::VBlank:
    interrupt = (stat & 0x10);
    bus.requestInterrupt(Bus::INTERRUPT_VBLANK);
    frameReady = true;
    break;
  case Mode::OAMSearch:
    interrupt = (stat & 0x20);
    break;
  default:
    break;
  }

  if (interrupt) {
    bus.requestInterrupt(Bus::INTERRUPT_STAT);
  }
}

void PPU::updateStatus() {
  if (currentScanline == lyc) {
    stat |= 0x04;
    if (stat & 0x40) {
      bus.requestInterrupt(Bus::INTERRUPT_STAT);
    }
  } else {
    stat &= ~0x04;
  }
}

void PPU::renderScanline() {
  // LCD Enable check
  if ((lcdc & 0x80) == 0)
    return;

  // Background rendering
  if (lcdc & 0x01) {
    uint16_t tileMap = (lcdc & 0x08) ? 0x9C00 : 0x9800;
    uint16_t tileData = (lcdc & 0x10) ? 0x8000 : 0x8800;
    bool unsig = (lcdc & 0x10) != 0;

    bool windowVisible = (lcdc & 0x20) && (currentScanline >= wy);

    bool windowUsedOnLine = false;

    for (int pixel = 0; pixel < 160; ++pixel) {
      int windowX = static_cast<int>(wx) - 7;
      bool isWindow = windowVisible && (pixel >= windowX);

      uint16_t currentTileMap = tileMap;
      uint8_t xPos, yPos;

      if (isWindow) {
        currentTileMap = (lcdc & 0x40) ? 0x9C00 : 0x9800;
        xPos = pixel - windowX;
        yPos = windowLineCounter;
        windowUsedOnLine = true;
      } else {
        xPos = pixel + scx;
        yPos = currentScanline + scy;
      }

      uint16_t tileRow = (yPos / 8) * 32;
      uint16_t tileCol = xPos / 8;
      uint16_t tileAddr = currentTileMap + tileRow + tileCol;

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

      int colorBit = 7 - (xPos % 8);

      uint8_t colorNum = ((data2 >> colorBit) & 1) << 1;
      colorNum |= ((data1 >> colorBit) & 1);

      uint8_t color = (bgp >> (colorNum * 2)) & 3;
      frameBuffer[currentScanline * 160 + pixel] = color;
    }

    if (windowUsedOnLine) {
      windowLineCounter++;
    }
  } else {
    // If BG is disabled, fill with color 0 (white)
    for (int pixel = 0; pixel < 160; ++pixel) {
      frameBuffer[currentScanline * 160 + pixel] = 0;
    }
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

uint8_t PPU::readOAM(uint16_t address) const {
  Mode mode = getMode();
  if (mode == Mode::OAMSearch || mode == Mode::PixelTransfer) {
    return 0xFF;
  }
  return oam[address - 0xFE00];
}

void PPU::writeOAM(uint16_t address, uint8_t value) {
  Mode mode = getMode();
  if (mode == Mode::OAMSearch || mode == Mode::PixelTransfer) {
    return;
  }
  oam[address - 0xFE00] = value;
}

uint8_t PPU::read(uint16_t address) const {
  if (getMode() == Mode::PixelTransfer) {
    return 0xFF;
  }
  return vram[(vramBank * 0x2000) + (address - 0x8000)];
}

void PPU::write(uint16_t address, uint8_t value) {
  if (getMode() == Mode::PixelTransfer) {
    return;
  }
  vram[(vramBank * 0x2000) + (address - 0x8000)] = value;
}

uint8_t PPU::readReg(uint16_t address) const {
  switch (address) {
  case 0xFF40:
    return lcdc;
  case 0xFF41:
    return stat | 0x80;
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
  case 0xFF4F:
    return vramBank | 0xFE;
  case 0xFF68:
    return bgPaletteIndex;
  case 0xFF69:
    return bgPalettes[bgPaletteIndex & 0x3F];
  case 0xFF6A:
    return objPaletteIndex;
  case 0xFF6B:
    return objPalettes[objPaletteIndex & 0x3F];
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
  case 0xFF4F:
    vramBank = value & 0x01;
    break;
  case 0xFF68:
    bgPaletteIndex = value;
    break;
  case 0xFF69:
    bgPalettes[bgPaletteIndex & 0x3F] = value;
    if (bgPaletteIndex & 0x80) {
      uint8_t nextIdx = (bgPaletteIndex & 0x3F) + 1;
      bgPaletteIndex = (bgPaletteIndex & 0x80) | (nextIdx & 0x3F);
    }
    break;
  case 0xFF6A:
    objPaletteIndex = value;
    break;
  case 0xFF6B:
    objPalettes[objPaletteIndex & 0x3F] = value;
    if (objPaletteIndex & 0x80) {
      uint8_t nextIdx = (objPaletteIndex & 0x3F) + 1;
      objPaletteIndex = (objPaletteIndex & 0x80) | (nextIdx & 0x3F);
    }
    break;
  }
}

void PPU::serialize(std::ostream &out) const {
  out.write(reinterpret_cast<const char *>(vram.data()), vram.size());
  out.write(reinterpret_cast<const char *>(oam.data()), oam.size());
  out.write(reinterpret_cast<const char *>(&vramBank), sizeof(vramBank));
  out.write(reinterpret_cast<const char *>(&lcdc), sizeof(lcdc));
  out.write(reinterpret_cast<const char *>(&stat), sizeof(stat));
  out.write(reinterpret_cast<const char *>(&scy), sizeof(scy));
  out.write(reinterpret_cast<const char *>(&scx), sizeof(scx));
  out.write(reinterpret_cast<const char *>(&lyc), sizeof(lyc));
  out.write(reinterpret_cast<const char *>(&bgp), sizeof(bgp));
  out.write(reinterpret_cast<const char *>(&obp0), sizeof(obp0));
  out.write(reinterpret_cast<const char *>(&obp1), sizeof(obp1));
  out.write(reinterpret_cast<const char *>(&wy), sizeof(wy));
  out.write(reinterpret_cast<const char *>(&wx), sizeof(wx));
  out.write(reinterpret_cast<const char *>(bgPalettes.data()),
            bgPalettes.size());
  out.write(reinterpret_cast<const char *>(objPalettes.data()),
            objPalettes.size());
  out.write(reinterpret_cast<const char *>(&bgPaletteIndex),
            sizeof(bgPaletteIndex));
  out.write(reinterpret_cast<const char *>(&objPaletteIndex),
            sizeof(objPaletteIndex));
  out.write(reinterpret_cast<const char *>(&scanlineCounter),
            sizeof(scanlineCounter));
  out.write(reinterpret_cast<const char *>(&currentScanline),
            sizeof(currentScanline));
  out.write(reinterpret_cast<const char *>(&windowLineCounter),
            sizeof(windowLineCounter));
  out.write(reinterpret_cast<const char *>(&mode3Duration),
            sizeof(mode3Duration));
}

void PPU::deserialize(std::istream &in) {
  in.read(reinterpret_cast<char *>(vram.data()), vram.size());
  in.read(reinterpret_cast<char *>(oam.data()), oam.size());
  in.read(reinterpret_cast<char *>(&vramBank), sizeof(vramBank));
  in.read(reinterpret_cast<char *>(&lcdc), sizeof(lcdc));
  in.read(reinterpret_cast<char *>(&stat), sizeof(stat));
  in.read(reinterpret_cast<char *>(&scy), sizeof(scy));
  in.read(reinterpret_cast<char *>(&scx), sizeof(scx));
  in.read(reinterpret_cast<char *>(&lyc), sizeof(lyc));
  in.read(reinterpret_cast<char *>(&bgp), sizeof(bgp));
  in.read(reinterpret_cast<char *>(&obp0), sizeof(obp0));
  in.read(reinterpret_cast<char *>(&obp1), sizeof(obp1));
  in.read(reinterpret_cast<char *>(&wy), sizeof(wy));
  in.read(reinterpret_cast<char *>(&wx), sizeof(wx));
  in.read(reinterpret_cast<char *>(bgPalettes.data()), bgPalettes.size());
  in.read(reinterpret_cast<char *>(objPalettes.data()), objPalettes.size());
  in.read(reinterpret_cast<char *>(&bgPaletteIndex), sizeof(bgPaletteIndex));
  in.read(reinterpret_cast<char *>(&objPaletteIndex), sizeof(objPaletteIndex));
  in.read(reinterpret_cast<char *>(&scanlineCounter), sizeof(scanlineCounter));
  in.read(reinterpret_cast<char *>(&currentScanline), sizeof(currentScanline));
  in.read(reinterpret_cast<char *>(&windowLineCounter),
          sizeof(windowLineCounter));
  in.read(reinterpret_cast<char *>(&mode3Duration), sizeof(mode3Duration));
}
