#include "Bus.h"

Bus::Bus() {
  // Initialize memory to 0
  memory.fill(0);
}

Bus::~Bus() {}

uint8_t Bus::read(uint16_t address) const { return memory[address]; }

void Bus::write(uint16_t address, uint8_t value) { memory[address] = value; }
