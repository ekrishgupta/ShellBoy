#pragma once
#include <cstdint>
#include <string>
#include <vector>

class Cartridge {
public:
  Cartridge();
  ~Cartridge();

  bool loadRom(const std::string &filepath);

  // Abstracted away Memory Bank Controller logic
  uint8_t read(uint16_t address) const;
  void write(uint16_t address, uint8_t value);

private:
  std::vector<uint8_t> rom;
  std::vector<uint8_t> ram;
  int mbcType = 0; // 0: None, 1: MBC1, 2: MBC2, 3: MBC3, etc.

  // Banking state
  int romBank = 1;
  int ramBank = 0;
  bool ramEnabled = false;
  int bankingMode = 0; // 0: ROM mode, 1: RAM mode (MBC1)

  // MBC3 RTC
  uint8_t rtcRegisters[5] = {0}; // S, M, H, DL, DH
  int rtcMappedBank = 0;
  bool rtcLatch = false;

  // MBC5
  int romBankHigh = 0;
};
