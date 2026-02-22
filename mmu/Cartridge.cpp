#include "Cartridge.h"

Cartridge::Cartridge() {}
Cartridge::~Cartridge() {}

bool Cartridge::loadRom(const std::string &filepath) {
  // Stub
  return true;
}

uint8_t Cartridge::read(uint16_t address) const {
  // Stub
  return 0xFF;
}

void Cartridge::write(uint16_t address, uint8_t value) {
  // Stub
}
