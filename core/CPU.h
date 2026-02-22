#pragma once

#include "Bus.h"
#include <cstdint>

// The Game Boy CPU (LR35902) uses 8-bit registers that can be combined
// into 16-bit registers (AF, BC, DE, HL). It is little-endian.
// On little-endian hosts, the low byte comes first in the struct.
union Register {
  uint16_t reg16;
  struct {
    uint8_t lo;
    uint8_t hi;
  };
};

class CPU {
public:
  explicit CPU(Bus &bus);
  ~CPU();

  void reset();
  int tick(); // Execute one instruction, return T-cycles elapsed

  // Registers
  Register AF; // A = hi, F = lo
  Register BC; // B = hi, C = lo
  Register DE; // D = hi, E = lo
  Register HL; // H = hi, L = lo

  uint16_t SP;
  uint16_t PC;

  bool IME; // Interrupt Master Enable

private:
  Bus &bus;

  uint8_t fetch();
  int execute(uint8_t opcode);
};
