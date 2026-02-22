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

void CPU::handleInterrupts() {
  uint8_t ie = bus.read(0xFFFF);
  uint8_t if_reg = bus.read(0xFF0F);
  uint8_t pending = ie & if_reg;

  if (pending == 0)
    return;

  halted = false;

  if (!IME)
    return;

  // Interrupts priority: V-Blank > LCD Stat > Timer > Serial > Joypad
  for (int i = 0; i < 5; ++i) {
    if ((pending >> i) & 0x01) {
      IME = false;
      bus.write(0xFF0F, if_reg & ~(1 << i)); // Clear the specific interrupt bit
      pushStack(PC);
      PC = 0x40 + (i * 0x08);
      break;
    }
  }
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
  handleInterrupts();

  if (halted) {
    return 4; // CPU in HALT still consumes cycles (mostly)
  }

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
  case 0x02: // LD (BC), A
    bus.write(BC.reg16, AF.hi);
    return 8;
  case 0x0A: // LD A, (BC)
    AF.hi = bus.read(BC.reg16);
    return 8;
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
  case 0x07: { // RLCA
    bool c = (AF.hi & 0x80) != 0;
    AF.hi = (AF.hi << 1) | (c ? 1 : 0);
    setFlag(FLAG_Z, false);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, c);
    return 4;
  }
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
  case 0x0F: { // RRCA
    bool c = (AF.hi & 0x01) != 0;
    AF.hi = (AF.hi >> 1) | (c ? 0x80 : 0);
    setFlag(FLAG_Z, false);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, c);
    return 4;
  }
  case 0x11: // LD DE, n16
    DE.reg16 = fetch16();
    return 12;
  case 0x12: // LD (DE), A
    bus.write(DE.reg16, AF.hi);
    return 8;
  case 0x1A: // LD A, (DE)
    AF.hi = bus.read(DE.reg16);
    return 8;
  case 0x18: { // JR n8
    int8_t offset = static_cast<int8_t>(fetch());
    PC += offset;
    return 12;
  }
  case 0x20: { // JR NZ, n8
    int8_t offset = static_cast<int8_t>(fetch());
    if (!getFlag(FLAG_Z)) {
      PC += offset;
      return 12;
    }
    return 8;
  }
  case 0x28: { // JR Z, n8
    int8_t offset = static_cast<int8_t>(fetch());
    if (getFlag(FLAG_Z)) {
      PC += offset;
      return 12;
    }
    return 8;
  }
  case 0x30: { // JR NC, n8
    int8_t offset = static_cast<int8_t>(fetch());
    if (!getFlag(FLAG_C)) {
      PC += offset;
      return 12;
    }
    return 8;
  }
  case 0x38: { // JR C, n8
    int8_t offset = static_cast<int8_t>(fetch());
    if (getFlag(FLAG_C)) {
      PC += offset;
      return 12;
    }
    return 8;
  }
  case 0x16: // LD D, n8
    DE.hi = fetch();
    return 8;
  case 0x1E: // LD E, n8
    DE.lo = fetch();
    return 8;
  case 0x21: // LD HL, n16
    HL.reg16 = fetch16();
    return 12;
  case 0x22: // LDI (HL+), A
    bus.write(HL.reg16, AF.hi);
    HL.reg16++;
    return 8;
  case 0x2A: // LDI A, (HL+)
    AF.hi = bus.read(HL.reg16);
    HL.reg16++;
    return 8;
  case 0x26: // LD H, n8
    HL.hi = fetch();
    return 8;
  case 0x2E: // LD L, n8
    HL.lo = fetch();
    return 8;
  case 0x31: // LD SP, n16
    SP = fetch16();
    return 12;
  case 0x32: // LDD (HL-), A
    bus.write(HL.reg16, AF.hi);
    HL.reg16--;
    return 8;
  case 0x3A: // LDD A, (HL-)
    AF.hi = bus.read(HL.reg16);
    HL.reg16--;
    return 8;
  case 0x36: // LD (HL), n8
    bus.write(HL.reg16, fetch());
    return 12;
  case 0x3E: // LD A, n8
    AF.hi = fetch();
    return 8;
  case 0x46: // LD B, (HL)
    BC.hi = bus.read(HL.reg16);
    return 8;
  case 0x4E: // LD C, (HL)
    BC.lo = bus.read(HL.reg16);
    return 8;
  case 0x56: // LD D, (HL)
    DE.hi = bus.read(HL.reg16);
    return 8;
  case 0x5E: // LD E, (HL)
    DE.lo = bus.read(HL.reg16);
    return 8;
  case 0x66: // LD H, (HL)
    HL.hi = bus.read(HL.reg16);
    return 8;
  case 0x6E: // LD L, (HL)
    HL.lo = bus.read(HL.reg16);
    return 8;
  case 0x7E: // LD A, (HL)
    AF.hi = bus.read(HL.reg16);
    return 8;
  case 0x70: // LD (HL), B
    bus.write(HL.reg16, BC.hi);
    return 8;
  case 0x71: // LD (HL), C
    bus.write(HL.reg16, BC.lo);
    return 8;
  case 0x72: // LD (HL), D
    bus.write(HL.reg16, DE.hi);
    return 8;
  case 0x73: // LD (HL), E
    bus.write(HL.reg16, DE.lo);
    return 8;
  case 0x74: // LD (HL), H
    bus.write(HL.reg16, HL.hi);
    return 8;
  case 0x75: // LD (HL), L
    bus.write(HL.reg16, HL.lo);
    return 8;
  case 0x77: // LD (HL), A
    bus.write(HL.reg16, AF.hi);
    return 8;
  case 0x80: { // ADD A, B
    uint8_t val = BC.hi;
    uint16_t res = AF.hi + val;
    setFlag(FLAG_H, (AF.hi & 0x0F) + (val & 0x0F) > 0x0F);
    AF.hi = res & 0xFF;
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_C, res > 0xFF);
    return 4;
  }
  case 0x81: { // ADD A, C
    uint8_t val = BC.lo;
    uint16_t res = AF.hi + val;
    setFlag(FLAG_H, (AF.hi & 0x0F) + (val & 0x0F) > 0x0F);
    AF.hi = res & 0xFF;
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_C, res > 0xFF);
    return 4;
  }
  case 0x82: { // ADD A, D
    uint8_t val = DE.hi;
    uint16_t res = AF.hi + val;
    setFlag(FLAG_H, (AF.hi & 0x0F) + (val & 0x0F) > 0x0F);
    AF.hi = res & 0xFF;
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_C, res > 0xFF);
    return 4;
  }
  case 0x83: { // ADD A, E
    uint8_t val = DE.lo;
    uint16_t res = AF.hi + val;
    setFlag(FLAG_H, (AF.hi & 0x0F) + (val & 0x0F) > 0x0F);
    AF.hi = res & 0xFF;
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_C, res > 0xFF);
    return 4;
  }
  case 0x84: { // ADD A, H
    uint8_t val = HL.hi;
    uint16_t res = AF.hi + val;
    setFlag(FLAG_H, (AF.hi & 0x0F) + (val & 0x0F) > 0x0F);
    AF.hi = res & 0xFF;
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_C, res > 0xFF);
    return 4;
  }
  case 0x85: { // ADD A, L
    uint8_t val = HL.lo;
    uint16_t res = AF.hi + val;
    setFlag(FLAG_H, (AF.hi & 0x0F) + (val & 0x0F) > 0x0F);
    AF.hi = res & 0xFF;
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_C, res > 0xFF);
    return 4;
  }
  case 0x86: { // ADD A, (HL)
    uint8_t val = bus.read(HL.reg16);
    uint16_t res = AF.hi + val;
    setFlag(FLAG_H, (AF.hi & 0x0F) + (val & 0x0F) > 0x0F);
    AF.hi = res & 0xFF;
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_C, res > 0xFF);
    return 8;
  }
  case 0x87: { // ADD A, A
    uint16_t res = AF.hi + AF.hi;
    setFlag(FLAG_H, (AF.hi & 0x0F) + (AF.hi & 0x0F) > 0x0F);
    AF.hi = res & 0xFF;
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_C, res > 0xFF);
    return 4;
  }

  case 0x90: { // SUB A, B
    uint8_t val = BC.hi;
    uint16_t res = AF.hi - val;
    setFlag(FLAG_H, (AF.hi & 0x0F) < (val & 0x0F));
    AF.hi = res & 0xFF;
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_C, res > 0xFF);
    return 4;
  }
  case 0x91: { // SUB A, C
    uint8_t val = BC.lo;
    uint16_t res = AF.hi - val;
    setFlag(FLAG_H, (AF.hi & 0x0F) < (val & 0x0F));
    AF.hi = res & 0xFF;
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_C, res > 0xFF);
    return 4;
  }
  case 0x92: { // SUB A, D
    uint8_t val = DE.hi;
    uint16_t res = AF.hi - val;
    setFlag(FLAG_H, (AF.hi & 0x0F) < (val & 0x0F));
    AF.hi = res & 0xFF;
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_C, res > 0xFF);
    return 4;
  }
  case 0x93: { // SUB A, E
    uint8_t val = DE.lo;
    uint16_t res = AF.hi - val;
    setFlag(FLAG_H, (AF.hi & 0x0F) < (val & 0x0F));
    AF.hi = res & 0xFF;
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_C, res > 0xFF);
    return 4;
  }
  case 0x94: { // SUB A, H
    uint8_t val = HL.hi;
    uint16_t res = AF.hi - val;
    setFlag(FLAG_H, (AF.hi & 0x0F) < (val & 0x0F));
    AF.hi = res & 0xFF;
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_C, res > 0xFF);
    return 4;
  }
  case 0x95: { // SUB A, L
    uint8_t val = HL.lo;
    uint16_t res = AF.hi - val;
    setFlag(FLAG_H, (AF.hi & 0x0F) < (val & 0x0F));
    AF.hi = res & 0xFF;
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_C, res > 0xFF);
    return 4;
  }
  case 0x96: { // SUB A, (HL)
    uint8_t val = bus.read(HL.reg16);
    uint16_t res = AF.hi - val;
    setFlag(FLAG_H, (AF.hi & 0x0F) < (val & 0x0F));
    AF.hi = res & 0xFF;
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_C, res > 0xFF);
    return 8;
  }
  case 0x97: { // SUB A, A
    AF.hi = 0;
    setFlag(FLAG_Z, true);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    return 4;
  }

  case 0xA0: // AND A, B
    AF.hi &= BC.hi;
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    setFlag(FLAG_C, false);
    return 4;
  case 0xA1: // AND A, C
    AF.hi &= BC.lo;
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    setFlag(FLAG_C, false);
    return 4;
  case 0xA2: // AND A, D
    AF.hi &= DE.hi;
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    setFlag(FLAG_C, false);
    return 4;
  case 0xA3: // AND A, E
    AF.hi &= DE.lo;
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    setFlag(FLAG_C, false);
    return 4;
  case 0xA4: // AND A, H
    AF.hi &= HL.hi;
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    setFlag(FLAG_C, false);
    return 4;
  case 0xA5: // AND A, L
    AF.hi &= HL.lo;
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    setFlag(FLAG_C, false);
    return 4;
  case 0xA6: // AND A, (HL)
    AF.hi &= bus.read(HL.reg16);
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    setFlag(FLAG_C, false);
    return 8;
  case 0xA7: // AND A, A
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    setFlag(FLAG_C, false);
    return 4;

  case 0xA8: // XOR A, B
    AF.hi ^= BC.hi;
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    return 4;
  case 0xA9: // XOR A, C
    AF.hi ^= BC.lo;
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    return 4;
  case 0xAA: // XOR A, D
    AF.hi ^= DE.hi;
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    return 4;
  case 0xAB: // XOR A, E
    AF.hi ^= DE.lo;
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    return 4;
  case 0xAC: // XOR A, H
    AF.hi ^= HL.hi;
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    return 4;
  case 0xAD: // XOR A, L
    AF.hi ^= HL.lo;
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    return 4;
  case 0xAE: // XOR A, (HL)
    AF.hi ^= bus.read(HL.reg16);
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    return 8;
  case 0xAF: // XOR A
    AF.hi = 0;
    setFlag(FLAG_Z, true);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    return 4;

  case 0xB0: // OR A, B
    AF.hi |= BC.hi;
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    return 4;
  case 0xB1: // OR A, C
    AF.hi |= BC.lo;
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    return 4;
  case 0xB2: // OR A, D
    AF.hi |= DE.hi;
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    return 4;
  case 0xB3: // OR A, E
    AF.hi |= DE.lo;
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    return 4;
  case 0xB4: // OR A, H
    AF.hi |= HL.hi;
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    return 4;
  case 0xB5: // OR A, L
    AF.hi |= HL.lo;
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    return 4;
  case 0xB6: // OR A, (HL)
    AF.hi |= bus.read(HL.reg16);
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    return 8;
  case 0xB7: // OR A, A
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    return 4;

  case 0xB8: { // CP A, B
    uint8_t val = BC.hi;
    uint16_t res = AF.hi - val;
    setFlag(FLAG_Z, (res & 0xFF) == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (AF.hi & 0x0F) < (val & 0x0F));
    setFlag(FLAG_C, res > 0xFF);
    return 4;
  }
  case 0xB9: { // CP A, C
    uint8_t val = BC.lo;
    uint16_t res = AF.hi - val;
    setFlag(FLAG_Z, (res & 0xFF) == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (AF.hi & 0x0F) < (val & 0x0F));
    setFlag(FLAG_C, res > 0xFF);
    return 4;
  }
  case 0xBA: { // CP A, D
    uint8_t val = DE.hi;
    uint16_t res = AF.hi - val;
    setFlag(FLAG_Z, (res & 0xFF) == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (AF.hi & 0x0F) < (val & 0x0F));
    setFlag(FLAG_C, res > 0xFF);
    return 4;
  }
  case 0xBB: { // CP A, E
    uint8_t val = DE.lo;
    uint16_t res = AF.hi - val;
    setFlag(FLAG_Z, (res & 0xFF) == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (AF.hi & 0x0F) < (val & 0x0F));
    setFlag(FLAG_C, res > 0xFF);
    return 4;
  }
  case 0xBC: { // CP A, H
    uint8_t val = HL.hi;
    uint16_t res = AF.hi - val;
    setFlag(FLAG_Z, (res & 0xFF) == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (AF.hi & 0x0F) < (val & 0x0F));
    setFlag(FLAG_C, res > 0xFF);
    return 4;
  }
  case 0xBD: { // CP A, L
    uint8_t val = HL.lo;
    uint16_t res = AF.hi - val;
    setFlag(FLAG_Z, (res & 0xFF) == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (AF.hi & 0x0F) < (val & 0x0F));
    setFlag(FLAG_C, res > 0xFF);
    return 4;
  }
  case 0xBE: { // CP A, (HL)
    uint8_t val = bus.read(HL.reg16);
    uint16_t res = AF.hi - val;
    setFlag(FLAG_Z, (res & 0xFF) == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (AF.hi & 0x0F) < (val & 0x0F));
    setFlag(FLAG_C, res > 0xFF);
    return 8;
  }
  case 0xBF: { // CP A, A
    setFlag(FLAG_Z, true);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    return 4;
  }
  case 0xC6: { // ADD A, n8
    uint8_t val = fetch();
    uint16_t res = AF.hi + val;
    setFlag(FLAG_H, (AF.hi & 0x0F) + (val & 0x0F) > 0x0F);
    AF.hi = res & 0xFF;
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_C, res > 0xFF);
    return 8;
  }
  case 0xD6: { // SUB A, n8
    uint8_t val = fetch();
    uint16_t res = AF.hi - val;
    setFlag(FLAG_H, (AF.hi & 0x0F) < (val & 0x0F));
    AF.hi = res & 0xFF;
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_C, res > 0xFF); // Borrow occurred
    return 8;
  }
  case 0xE6: { // AND A, n8
    AF.hi &= fetch();
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    setFlag(FLAG_C, false);
    return 8;
  }
  case 0xEE: { // XOR A, n8
    AF.hi ^= fetch();
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    return 8;
  }
  case 0xF6: { // OR A, n8
    AF.hi |= fetch();
    setFlag(FLAG_Z, AF.hi == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    return 8;
  }
  case 0xFE: { // CP A, n8
    uint8_t val = fetch();
    uint16_t res = AF.hi - val;
    setFlag(FLAG_Z, (res & 0xFF) == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (AF.hi & 0x0F) < (val & 0x0F));
    setFlag(FLAG_C, res > 0xFF);
    return 8;
  }
  case 0xC3: // JP nn
    PC = fetch16();
    return 16;
  case 0xCD: { // CALL nn
    uint16_t addr = fetch16();
    pushStack(PC);
    PC = addr;
    return 24;
  }
  case 0xC9: // RET
    PC = popStack();
    return 16;
  case 0xF3: // DI
    IME = false;
    return 4;
  case 0xFB: // EI
    IME = true;
    return 4;
  default:
    // Placeholder for unimplemented opcodes
    return 0;
  }
}
