#pragma once
#include <cstdint>

class Bus;

class Timer {
public:
  explicit Timer(Bus &bus);
  void tick(int cycles);

  uint8_t read(uint16_t address) const;
  void write(uint16_t address, uint8_t value);

private:
  Bus &bus;

  uint16_t div_internal = 0; // Internal 16-bit counter for DIV
  uint8_t tima = 0;          // TIMA register (0xFF05)
  uint8_t tma = 0;           // TMA register (0xFF06)
  uint8_t tac = 0;           // TAC register (0xFF07)
};
