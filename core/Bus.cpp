#include "Bus.h"
#include "core/Joypad.h"
#include "core/PPU.h"
#include "core/Timer.h"
#include "mmu/Cartridge.h"

Bus::Bus() {
  // Initialize memory to 0
  memory.fill(0);
}

Bus::~Bus() {}

uint8_t Bus::read(uint16_t address) const {
  if (address == 0xFF00) {
    if (joypad)
      return joypad->read();
  } else if (address >= ROM0_START && address <= ROM0_END) {
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
  } else if (address >= OAM_START && address <= OAM_END) {
    if (ppu)
      return ppu->readOAM(address);
  } else if (address >= 0xFF04 && address <= 0xFF07) {
    if (timer)
      return timer->read(address);
  } else if (address == 0xFF46) {
    return memory[0xFF46];
  } else if (address >= 0xFF40 && address <= 0xFF4B) {
    if (ppu)
      return ppu->readReg(address);
  }
  return memory[address];
}

void Bus::write(uint16_t address, uint8_t value) {
  if (address == 0xFF00) {
    if (joypad)
      joypad->write(value);
    return;
  } else if (address >= ROM0_START && address <= ROM0_END) {
    if (cartridge)
      cartridge->write(address, value);
    return;
  } else if (address >= ROMX_START && address <= ROMX_END) {
    if (cartridge)
      cartridge->write(address, value);
    return;
  } else if (address >= VRAM_START && address <= VRAM_END) {
    if (ppu)
      ppu->write(address, value);
    return;
  } else if (address >= SRAM_START && address <= SRAM_END) {
    if (cartridge)
      cartridge->write(address, value);
    return;
  } else if (address >= ECHO_START && address <= ECHO_END) {
    memory[address - 0x2000] = value;
    return;
  } else if (address >= OAM_START && address <= OAM_END) {
    if (ppu)
      ppu->writeOAM(address, value);
    return;
  } else if (address >= 0xFF04 && address <= 0xFF07) {
    if (timer)
      timer->write(address, value);
    return;
  } else if (address == 0xFF46) {
    // OAM DMA Transfer
    uint16_t source = static_cast<uint16_t>(value) << 8;
    for (uint16_t i = 0; i < 0xA0; i++) {
      write(0xFE00 + i, read(source + i));
    }
    memory[0xFF46] = value;
    return;
  } else if (address >= 0xFF40 && address <= 0xFF4B) {
    if (ppu)
      ppu->writeReg(address, value);
    return;
  }
  memory[address] = value;
}

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

void Bus::setTimer(Timer *t) { timer = t; }
void Bus::setJoypad(Joypad *j) { joypad = j; }

void Bus::requestInterrupt(uint8_t interrupt) {
  uint8_t if_reg = read(0xFF0F);
  write(0xFF0F, if_reg | interrupt);
}
