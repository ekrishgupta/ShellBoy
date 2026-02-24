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
    uint8_t type = rom[0x147];
    if (type >= 0x01 && type <= 0x03)
      mbcType = 1; // MBC1
    else if (type >= 0x0F && type <= 0x13)
      mbcType = 3; // MBC3
    else if (type >= 0x19 && type <= 0x1E)
      mbcType = 5; // MBC5
    else
      mbcType = 0; // ROM ONLY or unimplemented

    switch (rom[0x149]) {
    case 1:
      ram.resize(2048);
      break;
    case 2:
      ram.resize(8192);
      break;
    case 3:
      ram.resize(32768);
      break;
    case 4:
      ram.resize(131072);
      break;
    case 5:
      ram.resize(65536);
      break;
    default:
      ram.resize(0);
      break;
    }
  }
  return true;
}

return false;
}

uint8_t Cartridge::read(uint16_t address) const {
  if (address < 0x4000) {
    return rom[address];
  } else if (address < 0x8000) {
    uint32_t bank = romBank;
    if (mbcType == 5) {
      bank |= (romBankHigh << 8);
    }
    uint32_t mapped = (bank * 0x4000) + (address - 0x4000);
    return rom[mapped % rom.size()];
  } else if (address >= 0xA000 && address <= 0xBFFF) {
    if (ramEnabled) {
      if (mbcType == 3 && ramBank >= 0x08 && ramBank <= 0x0C) {
        return rtcRegisters[ramBank - 0x08];
      }
      uint32_t mapped = (ramBank * 0x2000) + (address - 0xA000);
      if (mapped < ram.size())
        return ram[mapped];
    }
  }
  return 0xFF;
}

void Cartridge::write(uint16_t address, uint8_t value) {
  if (mbcType == 1) { // MBC1
    if (address < 0x2000) {
      ramEnabled = ((value & 0x0F) == 0x0A);
    } else if (address < 0x4000) {
      romBank = (romBank & 0xE0) | (value & 0x1F);
      if (romBank == 0 || romBank == 0x20 || romBank == 0x40 ||
          (romBank == 0x60)) {
        romBank++;
      }
    } else if (address < 0x6000) {
      if (bankingMode == 0) {
        romBank = (romBank & 0x1F) | ((value & 0x03) << 5);
        if (romBank == 0 || romBank == 0x20 || romBank == 0x40 ||
            (romBank == 0x60)) {
          romBank++;
        }
      } else {
        ramBank = value & 0x03;
      }
    } else if (address < 0x8000) {
      bankingMode = value & 0x01;
      if (bankingMode == 0) {
        ramBank = 0;
      }
    }
  } else if (mbcType == 3) { // MBC3
    if (address < 0x2000) {
      ramEnabled = ((value & 0x0F) == 0x0A);
    } else if (address < 0x4000) {
      romBank = value & 0x7F;
      if (romBank == 0)
        romBank = 1;
    } else if (address < 0x6000) {
      ramBank = value;
    } else if (address < 0x8000) {
      if (rtcLatch == 0 && value == 1) {
        // In a real implementation, we would capture the current time here.
        // For now, we just acknowledge the latch signal.
      }
      rtcLatch = value;
    }
  } else if (mbcType == 5) { // MBC5
    if (address < 0x2000) {
      ramEnabled = ((value & 0x0F) == 0x0A);
    } else if (address < 0x4000) {
      if (address < 0x3000) {
        romBank = value;
      } else {
        romBankHigh = value & 0x01;
      }
    } else if (address < 0x6000) {
      ramBank = value & 0x0F;
    }
  }

  // Common RAM write logic
  if (address >= 0xA000 && address <= 0xBFFF) {
    if (ramEnabled) {
      if (mbcType == 3 && ramBank >= 0x08 && ramBank <= 0x0C) {
        rtcRegisters[ramBank - 0x08] = value;
        return;
      }
      uint32_t mapped = (ramBank * 0x2000) + (address - 0xA000);
      if (mapped < ram.size()) {
        ram[mapped] = value;
      }
    } else if (mbcType == 0) {
      // ROM ONLY, ram might be written to if 0xA000 - 0xBFFF is accessed (e.g.,
      // Tetris)
      uint32_t offset = address - 0xA000;
      if (offset < ram.size()) {
        ram[offset] = value;
      }
    }
  }
}
