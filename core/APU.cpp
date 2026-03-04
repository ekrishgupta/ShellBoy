#include "APU.h"
#include <iostream>

APU::APU() {
  // Initialize registers with some default values if necessary
  for (int i = 0; i < 0x30; i++) {
    registers[i] = 0;
  }
}

APU::~APU() {}

uint8_t APU::readReg(uint16_t address) const {
  if (address >= 0xFF10 && address <= 0xFF3F) {
    return registers[address - 0xFF10];
  }
  return 0xFF;
}

void APU::writeReg(uint16_t address, uint8_t value) {
  if (address >= 0xFF10 && address <= 0xFF3F) {
    registers[address - 0xFF10] = value;
  }
}

void APU::tick(int tCycles) {
  // Emulate APU ticking if needed
  // This will be expanded later when full audio is implemented
}
