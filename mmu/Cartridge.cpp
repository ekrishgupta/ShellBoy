#include "Cartridge.h"
#include <fstream>

Cartridge::Cartridge() {}
Cartridge::~Cartridge() {
  saveBattery();
  saveRtc();
}

bool Cartridge::loadRom(const std::string &filepath) {
  std::ifstream file(filepath, std::ios::binary | std::ios::ate);
  if (!file.is_open())
    return false;

  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  rom.resize(size);
  if (file.read(reinterpret_cast<char *>(rom.data()), size)) {
    uint8_t type = rom[0x147];
    hasBattery =
        (type == 0x03 || type == 0x06 || type == 0x09 || type == 0x0F ||
         type == 0x10 || type == 0x13 || type == 0x1B || type == 0x1E);

    if (type >= 0x01 && type <= 0x03)
      mbcType = 1; // MBC1
    else if (type == 0x05 || type == 0x06)
      mbcType = 2; // MBC2
    else if (type >= 0x0F && type <= 0x13)
      mbcType = 3; // MBC3
    else if (type >= 0x19 && type <= 0x1E)
      mbcType = 5; // MBC5
    else
      mbcType = 0; // ROM ONLY or unimplemented

    size_t lastDot = filepath.find_last_of('.');
    if (lastDot != std::string::npos) {
      savePath = filepath.substr(0, lastDot) + ".sav";
    } else {
      savePath = filepath + ".sav";
    }

    if (mbcType == 2) {
      ram.resize(512);
    } else {
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
    loadBattery();
    loadRtc();
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
      if (mbcType == 2) {
        return ram[(address - 0xA000) % 512] | 0xF0;
      }
      if (mbcType == 3 && ramBank >= 0x08 && ramBank <= 0x0C) {
        return rtcLatched[ramBank - 0x08];
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
  } else if (mbcType == 2) { // MBC2
    if (address < 0x4000) {
      if ((address & 0x0100) == 0) {
        ramEnabled = ((value & 0x0F) == 0x0A);
      } else {
        romBank = value & 0x0F;
        if (romBank == 0)
          romBank = 1;
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
        updateRtc();
        for (int i = 0; i < 5; i++) {
          rtcLatched[i] = rtcRegisters[i];
        }
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
        updateRtc();
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

void Cartridge::saveBattery() {
  if (!hasBattery || ram.empty())
    return;
  std::ofstream file(savePath, std::ios::binary | std::ios::trunc);
  if (file.is_open()) {
    file.write(reinterpret_cast<const char *>(ram.data()), ram.size());
  }
}

void Cartridge::loadBattery() {
  if (!hasBattery || ram.empty())
    return;
  std::ifstream file(savePath, std::ios::binary);
  if (file.is_open()) {
    file.read(reinterpret_cast<char *>(ram.data()), ram.size());
  }
}

void Cartridge::updateRtc() {
  if (mbcType != 3)
    return;

  auto now = std::chrono::system_clock::now();
  uint64_t currentUnixTime =
      std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch())
          .count();
  int64_t elapsed = currentUnixTime - rtcLastTime;
  rtcLastTime = currentUnixTime;

  if (elapsed <= 0)
    return;

  if (rtcRegisters[4] & 0x40) {
    return;
  }

  uint64_t seconds = rtcRegisters[0] + elapsed;
  rtcRegisters[0] = seconds % 60;

  uint64_t minutes = rtcRegisters[1] + (seconds / 60);
  rtcRegisters[1] = minutes % 60;

  uint64_t hours = rtcRegisters[2] + (minutes / 60);
  rtcRegisters[2] = hours % 24;

  uint64_t daysElapsed = hours / 24;
  if (daysElapsed > 0) {
    uint32_t currentDays = rtcRegisters[3] | ((rtcRegisters[4] & 0x01) << 8);
    currentDays += daysElapsed;
    if (currentDays > 0x1FF) {
      rtcRegisters[4] |= 0x80;
      currentDays &= 0x1FF;
    }
    rtcRegisters[3] = currentDays & 0xFF;
    rtcRegisters[4] = (rtcRegisters[4] & 0xFE) | ((currentDays >> 8) & 0x01);
  }
}

void Cartridge::saveRtc() {
  if (mbcType != 3 || !hasBattery)
    return;
  std::string rtcPath = savePath.substr(0, savePath.find_last_of('.')) + ".rtc";
  std::ofstream file(rtcPath, std::ios::binary | std::ios::trunc);
  if (file.is_open()) {
    updateRtc();
    file.write(reinterpret_cast<const char *>(rtcRegisters), 5);
    file.write(reinterpret_cast<const char *>(rtcLatched), 5);
    file.write(reinterpret_cast<const char *>(&rtcLastTime),
               sizeof(rtcLastTime));
  }
}

void Cartridge::loadRtc() {
  if (mbcType != 3 || !hasBattery)
    return;
  std::string rtcPath = savePath.substr(0, savePath.find_last_of('.')) + ".rtc";
  std::ifstream file(rtcPath, std::ios::binary);
  if (file.is_open()) {
    file.read(reinterpret_cast<char *>(rtcRegisters), 5);
    file.read(reinterpret_cast<char *>(rtcLatched), 5);
    file.read(reinterpret_cast<char *>(&rtcLastTime), sizeof(rtcLastTime));
    updateRtc();
  } else {
    auto now = std::chrono::system_clock::now();
    rtcLastTime =
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch())
            .count();
  }
}

void Cartridge::serialize(std::ostream &out) const {
  out.write(reinterpret_cast<const char *>(ram.data()), ram.size());
  out.write(reinterpret_cast<const char *>(&romBank), sizeof(romBank));
  out.write(reinterpret_cast<const char *>(&ramBank), sizeof(ramBank));
  out.write(reinterpret_cast<const char *>(&ramEnabled), sizeof(ramEnabled));
  out.write(reinterpret_cast<const char *>(&bankingMode), sizeof(bankingMode));
  out.write(reinterpret_cast<const char *>(rtcRegisters), 5);
  out.write(reinterpret_cast<const char *>(rtcLatched), 5);
  out.write(reinterpret_cast<const char *>(&rtcLastTime), sizeof(rtcLastTime));
  out.write(reinterpret_cast<const char *>(&rtcMappedBank),
            sizeof(rtcMappedBank));
  out.write(reinterpret_cast<const char *>(&rtcLatch), sizeof(rtcLatch));
  out.write(reinterpret_cast<const char *>(&romBankHigh), sizeof(romBankHigh));
}

void Cartridge::deserialize(std::istream &in) {
  in.read(reinterpret_cast<char *>(ram.data()), ram.size());
  in.read(reinterpret_cast<char *>(&romBank), sizeof(romBank));
  in.read(reinterpret_cast<char *>(&ramBank), sizeof(ramBank));
  in.read(reinterpret_cast<char *>(&ramEnabled), sizeof(ramEnabled));
  in.read(reinterpret_cast<char *>(&bankingMode), sizeof(bankingMode));
  in.read(reinterpret_cast<char *>(rtcRegisters), 5);
  in.read(reinterpret_cast<char *>(rtcLatched), 5);
  in.read(reinterpret_cast<char *>(&rtcLastTime), sizeof(rtcLastTime));
  in.read(reinterpret_cast<char *>(&rtcMappedBank), sizeof(rtcMappedBank));
  in.read(reinterpret_cast<char *>(&rtcLatch), sizeof(rtcLatch));
  in.read(reinterpret_cast<char *>(&romBankHigh), sizeof(romBankHigh));
}
