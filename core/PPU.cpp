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
