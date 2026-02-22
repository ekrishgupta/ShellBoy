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
  int mbcType = 0;
};
