#include "Bus.h"
#include "core/Joypad.h"
#include "core/PPU.h"
#include "core/Serial.h"
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
  } else if (address == 0xFF01 || address == 0xFF02) {
    if (serial)
      return serial->read(address);
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
    return read(address - 0x2000);
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
  } else if (address == 0xFF4F) {
    if (ppu)
      return ppu->readReg(address);
  } else if (address == 0xFF51) {
    return hdma1;
  } else if (address == 0xFF52) {
    return hdma2;
  } else if (address == 0xFF53) {
    return hdma3;
  } else if (address == 0xFF54) {
    return hdma4;
  } else if (address == 0xFF55) {
    return hdmaActive ? (hdmaLength - 1) : 0xFF;
  } else if (address >= 0xFF68 && address <= 0xFF6B) {
    if (ppu)
      return ppu->readReg(address);
  } else if (address >= WRAM_START && address <= WRAM_END) {
    if (address < 0xD000) {
      return wram[address - 0xC000];
    } else {
      return wram[(wramBank * 0x1000) + (address - 0xD000)];
    }
  } else if (address == 0xFF70) {
    return wramBank;
  }
  return memory[address];
}

void Bus::write(uint16_t address, uint8_t value) {
  if (address == 0xFF00) {
    if (joypad)
      joypad->write(value);
    return;
  } else if (address == 0xFF01 || address == 0xFF02) {
    if (serial)
      serial->write(address, value);
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
    write(address - 0x2000, value);
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
  } else if (address == 0xFF4F) {
    if (ppu)
      ppu->writeReg(address, value);
    return;
  } else if (address == 0xFF51) {
    hdma1 = value;
    return;
  } else if (address == 0xFF52) {
    hdma2 = value & 0xF0;
    return;
  } else if (address == 0xFF53) {
    hdma3 = value & 0x1F;
    return;
  } else if (address == 0xFF54) {
    hdma4 = value & 0xF0;
    return;
  } else if (address == 0xFF55) {
    if (hdmaActive && (value & 0x80) == 0) {
      hdmaActive = false;
      hdma5 = value | 0x80;
    } else {
      hdmaLength = (value & 0x7F) + 1;
      hdmaSource = (hdma1 << 8) | hdma2;
      hdmaDest = 0x8000 | ((hdma3 << 8) | hdma4);
      if ((value & 0x80) == 0) {
        for (int i = 0; i < hdmaLength * 0x10; i++) {
          write(hdmaDest + i, read(hdmaSource + i));
        }
        hdmaActive = false;
        hdma5 = 0xFF;
      } else {
        hdmaActive = true;
        hdma5 = value & 0x7F;
      }
    }
    return;
  } else if (address >= 0xFF68 && address <= 0xFF6B) {
    if (ppu)
      ppu->writeReg(address, value);
    return;
  } else if (address >= WRAM_START && address <= WRAM_END) {
    if (address < 0xD000) {
      wram[address - 0xC000] = value;
    } else {
      wram[(wramBank * 0x1000) + (address - 0xD000)] = value;
    }
    return;
  } else if (address == 0xFF70) {
    wramBank = value & 0x07;
    if (wramBank == 0)
      wramBank = 1;
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
void Bus::setSerial(Serial *s) { serial = s; }

void Bus::requestInterrupt(uint8_t interrupt) {
  uint8_t if_reg = read(0xFF0F);
  write(0xFF0F, if_reg | interrupt);
}

void Bus::processHDMA() {
  if (!hdmaActive)
    return;

  for (int i = 0; i < 0x10; i++) {
    write(hdmaDest++, read(hdmaSource++));
  }
  hdmaLength--;

  if (hdmaLength == 0) {
    hdmaActive = false;
    hdma5 = 0xFF;
  }
}
