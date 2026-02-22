#include "Bus.h"

Bus::Bus() {
  // Initialize memory to 0
  memory.fill(0);
}

Bus::~Bus() {}

uint8_t Bus::read(uint16_t address) const { return memory[address]; }

void Bus::write(uint16_t address, uint8_t value) { memory[address] = value; }

uint16_t Bus::read16(uint16_t address) const {
  uint8_t lo = read(address);
  uint8_t hi = read(address + 1);
  return (static_cast<uint16_t>(hi) << 8) | lo;
}

void Bus::write16(uint16_t address, uint16_t value) {
  write(address, value & 0xFF);
  write(address + 1, value >> 8);
}

void Bus::setCartridge(Cartridge *cart) { cartridge = cart; }

void Bus::setPPU(PPU *pixel_unit) { ppu = pixel_unit; }
