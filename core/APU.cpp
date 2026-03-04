#include "APU.h"
#include <iostream>

APU::APU() {
  // Initialize registers with some default values if necessary
  for (int i = 0; i < 0x30; i++) {
    registers[i] = 0;
  }
}

APU::~APU() {}

uint8_t APU::readReg(uint16_t address) const {
  if (address >= 0xFF10 && address <= 0xFF3F) {
    return registers[address - 0xFF10];
  }
  return 0xFF;
}

void APU::writeReg(uint16_t address, uint8_t value) {
  if (address >= 0xFF10 && address <= 0xFF3F) {
    registers[address - 0xFF10] = value;

    // Handle channel triggering and specific register updates
    switch (address) {
    case 0xFF14: // NR14
      if (value & 0x80) {
        ch1.enabled = true;
        // Trigger logic: reset timers, envelopes, etc.
        if (ch1.lengthTimer == 0)
          ch1.lengthTimer = 64;
      }
      break;
    case 0xFF19: // NR24
      if (value & 0x80) {
        ch2.enabled = true;
        if (ch2.lengthTimer == 0)
          ch2.lengthTimer = 64;
      }
      break;
    case 0xFF1E: // NR34
      if (value & 0x80) {
        ch3.enabled = true;
        if (ch3.lengthTimer == 0)
          ch3.lengthTimer = 256;
      }
      break;
    case 0xFF23: // NR44
      if (value & 0x80) {
        ch4.enabled = true;
        if (ch4.lengthTimer == 0)
          ch4.lengthTimer = 64;
      }
      break;
    case 0xFF26: // NR52
      // Bit 7 is Master Power
      if (!(value & 0x80)) {
        // Power off: clear all registers and channels
        for (int i = 0; i < 0x20; i++)
          registers[i] = 0;
        ch1.enabled = ch2.enabled = ch3.enabled = ch4.enabled = false;
      }
      break;
    }
  }
}

void APU::tick(int tCycles) {
  // Master clock is 4.194304 MHz
  // Frame sequencer clocks at 512 Hz (every 8192 T-cycles)
  frameSequencerCounter += tCycles;
  if (frameSequencerCounter >= 8192) {
    frameSequencerCounter -= 8192;
    tickFrameSequencer();
  }

  // Channel ticking: decrease frequency timers
  // (Simplified for now)
}

void APU::tickFrameSequencer() {
  frameSequencerStep = (frameSequencerStep + 1) & 7;

  // Step 0: Length
  // Step 1: Nothing
  // Step 2: Length & Sweep
  // Step 3: Nothing
  // Step 4: Length
  // Step 5: Nothing
  // Step 6: Length & Sweep
  // Step 7: Envelope

  bool clockLength = (frameSequencerStep % 2 == 0);
  bool clockEnvelope = (frameSequencerStep == 7);
  bool clockSweep = (frameSequencerStep == 2 || frameSequencerStep == 6);

  if (clockLength) {
    if (ch1.lengthTimer > 0 && (registers[0xFF14 - 0xFF10] & 0x40)) {
      if (--ch1.lengthTimer == 0)
        ch1.enabled = false;
    }
    if (ch2.lengthTimer > 0 && (registers[0xFF19 - 0xFF10] & 0x40)) {
      if (--ch2.lengthTimer == 0)
        ch2.enabled = false;
    }
    // ... ch3, ch4 ...
  }
}
