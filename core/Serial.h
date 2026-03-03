#pragma once

#include <cstdint>
#include <iostream>

class Bus;

class Serial {
public:
  Serial(Bus *bus);
  ~Serial();

  uint8_t read(uint16_t address) const;
  void write(uint16_t address, uint8_t value);

  void tick(uint32_t cycles);

private:
  Bus *bus;
  uint8_t sb = 0x00; // Serial Transfer Data
  uint8_t sc = 0x7E; // Serial Transfer Control (Bit 0-6 are often 1 by default
                     // on some models, but let's stick to basics)

  // Transfer state
  bool transferring = false;
  uint32_t transfer_cycles = 0;
};
