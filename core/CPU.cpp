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

void CPU::pushStack(uint16_t value) {
  SP -= 2;
  bus.write16(SP, value);
}

uint16_t CPU::popStack() {
  uint16_t value = bus.read16(SP);
  SP += 2;
  return value;
}

uint8_t CPU::fetch() {
  uint8_t val = bus.read(PC);
  PC++;
  return val;
}

uint16_t CPU::fetch16() {
  uint16_t val = bus.read16(PC);
  PC += 2;
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
  case 0x00: // NOP
    return 4;
  case 0x01: // LD BC, n16
    BC.reg16 = fetch16();
    return 12;
  case 0x04: { // INC B
    bool h = (BC.hi & 0x0F) == 0x0F;
    BC.hi++;
    setFlag(FLAG_Z, BC.hi == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, h);
    return 4;
  }
  case 0x05: { // DEC B
    bool h = (BC.hi & 0x0F) == 0;
    BC.hi--;
    setFlag(FLAG_Z, BC.hi == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, h);
    return 4;
  }
  case 0x06: // LD B, n8
    BC.hi = fetch();
    return 8;
  case 0x0C: { // INC C
    bool h = (BC.lo & 0x0F) == 0x0F;
    BC.lo++;
    setFlag(FLAG_Z, BC.lo == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, h);
    return 4;
  }
  case 0x0D: { // DEC C
    bool h = (BC.lo & 0x0F) == 0;
    BC.lo--;
    setFlag(FLAG_Z, BC.lo == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, h);
    return 4;
  }
  case 0x0E: // LD C, n8
    BC.lo = fetch();
    return 8;
  case 0x11: // LD DE, n16
    DE.reg16 = fetch16();
    return 12;
  case 0x16: // LD D, n8
    DE.hi = fetch();
    return 8;
  case 0x1E: // LD E, n8
    DE.lo = fetch();
    return 8;
  case 0x21: // LD HL, n16
    HL.reg16 = fetch16();
    return 12;
  case 0x26: // LD H, n8
    HL.hi = fetch();
    return 8;
  case 0x2E: // LD L, n8
    HL.lo = fetch();
    return 8;
  case 0x31: // LD SP, n16
    SP = fetch16();
    return 12;
  case 0x3E: // LD A, n8
    AF.hi = fetch();
    return 8;
  default:
    // Placeholder for unimplemented opcodes
    return 0;
  }
}
