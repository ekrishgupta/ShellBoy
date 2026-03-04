#pragma once

#include <array>
#include <cstdint>

class APU {
public:
  APU();
  ~APU();

  uint8_t readReg(uint16_t address) const;
  void writeReg(uint16_t address, uint8_t value);

  void tick(int tCycles);

private:
  std::array<uint8_t, 0x30> registers{}; // 0xFF10 to 0xFF3F
};
