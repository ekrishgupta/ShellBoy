#pragma once

#include <array>
#include <cstdint>

class Bus {
public:
  Bus();
  ~Bus();

  uint8_t read(uint16_t address) const;
  void write(uint16_t address, uint8_t value);

  uint16_t read16(uint16_t address) const;
  void write16(uint16_t address, uint16_t value);

private:
  std::array<uint8_t, 0x10000> memory{}; // Simple 64KB memory map for now
};
