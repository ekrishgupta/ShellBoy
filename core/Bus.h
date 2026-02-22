#pragma once

#include <array>
#include <cstdint>

class Cartridge;
class PPU;
class Timer;

class Bus {
public:
  Bus();
  ~Bus();

  uint8_t read(uint16_t address) const;
  void write(uint16_t address, uint8_t value);

  uint16_t read16(uint16_t address) const;
  void write16(uint16_t address, uint16_t value);

  void setCartridge(Cartridge *cart);
  void setPPU(PPU *pixel_unit);
  void setTimer(Timer *t);

  // Interrupt Bit Constants
  static constexpr uint8_t INTERRUPT_VBLANK = 0x01;
  static constexpr uint8_t INTERRUPT_STAT = 0x02;
  static constexpr uint8_t INTERRUPT_TIMER = 0x04;
  static constexpr uint8_t INTERRUPT_SERIAL = 0x08;
  static constexpr uint8_t INTERRUPT_JOYPAD = 0x10;

  void requestInterrupt(uint8_t interrupt);

private:
  static constexpr uint16_t ROM0_START = 0x0000;
  static constexpr uint16_t ROM0_END = 0x3FFF;
  static constexpr uint16_t ROMX_START = 0x4000;
  static constexpr uint16_t ROMX_END = 0x7FFF;
  static constexpr uint16_t VRAM_START = 0x8000;
  static constexpr uint16_t VRAM_END = 0x9FFF;
  static constexpr uint16_t SRAM_START = 0xA000;
  static constexpr uint16_t SRAM_END = 0xBFFF;
  static constexpr uint16_t WRAM_START = 0xC000;
  static constexpr uint16_t WRAM_END = 0xDFFF;
  static constexpr uint16_t ECHO_START = 0xE000;
  static constexpr uint16_t ECHO_END = 0xFDFF;
  static constexpr uint16_t OAM_START = 0xFE00;
  static constexpr uint16_t OAM_END = 0xFE9F;
  static constexpr uint16_t FORBIDDEN_START = 0xFEA0;
  static constexpr uint16_t FORBIDDEN_END = 0xFEFF;
  static constexpr uint16_t IO_START = 0xFF00;
  static constexpr uint16_t IO_END = 0xFF7F;
  static constexpr uint16_t HRAM_START = 0xFF80;
  static constexpr uint16_t HRAM_END = 0xFFFE;
  static constexpr uint16_t IE_REG = 0xFFFF;

  std::array<uint8_t, 0x10000> memory{}; // Simple 64KB memory map for now

  Cartridge *cartridge = nullptr;
  PPU *ppu = nullptr;
  Timer *timer = nullptr;
};
