#include "Cartridge.h"
#include <fstream>

Cartridge::Cartridge() {}
Cartridge::~Cartridge() {}

bool Cartridge::loadRom(const std::string &filepath) {
  std::ifstream file(filepath, std::ios::binary | std::ios::ate);
  if (!file.is_open())
    return false;

  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  rom.resize(size);
  if (file.read(reinterpret_cast<char *>(rom.data()), size)) {
    return true;
  }

  return false;
}

uint8_t Cartridge::read(uint16_t address) const {
  if (address < rom.size())
    return rom[address];
  return 0xFF;
}

void Cartridge::write(uint16_t address, uint8_t value) {
  // Stub
}
