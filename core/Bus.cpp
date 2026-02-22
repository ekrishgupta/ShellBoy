#include "Bus.h"
#include "core/PPU.h"
#include "mmu/Cartridge.h"

Bus::Bus() {
  // Initialize memory to 0
  memory.fill(0);
}

Bus::~Bus() {}

uint8_t Bus::read(uint16_t address) const {
  if (address >= ROM0_START && address <= ROM0_END) {
    if (cartridge)
      return cartridge->read(address);
  } else if (address >= ROMX_START && address <= ROMX_END) {
    if (cartridge)
      return cartridge->read(address);
  } else if (address >= VRAM_START && address <= VRAM_END) {
    if (ppu)
      return ppu->read(address);
  } else if (address >= SRAM_START && address <= SRAM_END) {
    if (cartridge)
      return cartridge->read(address);
  } else if (address >= ECHO_START && address <= ECHO_END) {
    return memory[address - 0x2000];
  }
  return memory[address];
}

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
