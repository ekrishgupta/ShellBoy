#include "CPU.h"

CPU::CPU(Bus &b) : bus(b) { reset(); }

CPU::~CPU() {}

void CPU::reset() {
  // Standard initialization values for DMG-01
  AF.hi = 0x01;
  AF.lo = 0xB0;
  BC.reg16 = 0x0013;
  DE.reg16 = 0x00D8;
  HL.reg16 = 0x014D;
  SP = 0xFFFE;
  PC = 0x0100;
  IME = false;
}

void CPU::setFlag(uint8_t flag, bool value) {
  if (value) {
    AF.lo |= flag;
  } else {
    AF.lo &= ~flag;
  }
}

bool CPU::getFlag(uint8_t flag) const { return (AF.lo & flag) != 0; }

uint8_t CPU::fetch() {
  uint8_t val = bus.read(PC);
  PC++;
  return val;
}

int CPU::tick() {
  uint8_t opcode = fetch();
  return execute(opcode);
}

int CPU::execute(uint8_t opcode) {
  // Initial switch statement for execution.
  // 0x00 is NOP, which takes 4 T-cycles.
  switch (opcode) {
  case 0x00:
    return 4;
  default:
    // Placeholder for unimplemented opcodes
    return 0;
  }
}
