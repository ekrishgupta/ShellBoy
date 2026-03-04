#include "APU.h"
#include <iostream>

APU::APU() {
  nr50 = 0;
  nr51 = 0;
  nr52 = 0x80; // APU on by default?
}

APU::~APU() {}

uint8_t APU::readReg(uint16_t address) const {
  if (address >= 0xFF30 && address <= 0xFF3F) {
    return ch3.waveRAM[address - 0xFF30];
  }

  switch (address) {
  // Channel 1
  case 0xFF10:
    return ch1.nrX0 | 0x80;
  case 0xFF11:
    return ch1.nrX1 | 0x3F;
  case 0xFF12:
    return ch1.nrX2;
  case 0xFF13:
    return 0xFF;
  case 0xFF14:
    return ch1.nrX4 | 0xBF;

  // Channel 2
  case 0xFF16:
    return ch2.nrX1 | 0x3F;
  case 0xFF17:
    return ch2.nrX2;
  case 0xFF18:
    return 0xFF;
  case 0xFF19:
    return ch2.nrX4 | 0xBF;

  // Channel 3
  case 0xFF1A:
    return ch3.nr30 | 0x7F;
  case 0xFF1B:
    return 0xFF;
  case 0xFF1C:
    return ch3.nr32 | 0x9F;
  case 0xFF1D:
    return 0xFF;
  case 0xFF1E:
    return ch3.nr34 | 0xBF;

  // Channel 4
  case 0xFF20:
    return 0xFF;
  case 0xFF21:
    return ch4.nr42;
  case 0xFF22:
    return ch4.nr43;
  case 0xFF23:
    return ch4.nr44 | 0xBF;

  // Control
  case 0xFF24:
    return nr50;
  case 0xFF25:
    return nr51;
  case 0xFF26: {
    uint8_t res = (nr52 & 0x80) | 0x70;
    if (ch1.enabled)
      res |= 0x01;
    if (ch2.enabled)
      res |= 0x02;
    if (ch3.enabled)
      res |= 0x04;
    if (ch4.enabled)
      res |= 0x08;
    return res;
  }

  default:
    return 0xFF;
  }
}

void APU::writeReg(uint16_t address, uint8_t value) {
  if (!(nr52 & 0x80) && address != 0xFF26) {
    return; // APU off (except NR52)
  }

  if (address >= 0xFF30 && address <= 0xFF3F) {
    ch3.waveRAM[address - 0xFF30] = value;
    return;
  }

  switch (address) {
  // Channel 1
  case 0xFF10:
    ch1.nrX0 = value;
    break;
  case 0xFF11:
    ch1.nrX1 = value;
    ch1.lengthTimer = 64 - (value & 0x3F);
    break;
  case 0xFF12:
    ch1.nrX2 = value;
    break;
  case 0xFF13:
    ch1.nrX3 = value;
    ch1.timer = (ch1.timer & 0x700) | value;
    break;
  case 0xFF14:
    ch1.nrX4 = value;
    ch1.timer = (ch1.timer & 0xFF) | ((value & 0x07) << 8);
    if (value & 0x80) {
      ch1.enabled = true;
      if (ch1.lengthTimer == 0)
        ch1.lengthTimer = 64;

      // CH1 Sweep Trigger
      ch1.shadowFrequency = ((ch1.nrX4 & 0x07) << 8) | ch1.nrX3;
      int sweepPeriod = (ch1.nrX0 >> 4) & 0x07;
      int sweepShift = ch1.nrX0 & 0x07;
      ch1.sweepTimer = sweepPeriod == 0 ? 8 : sweepPeriod;
      ch1.sweepEnabled = (sweepPeriod != 0 || sweepShift != 0);
      if (sweepShift != 0) {
        calculateSweep(); // Check for immediate overflow
      }
    }
    break;

  // Channel 2
  case 0xFF16:
    ch2.nrX1 = value;
    ch2.lengthTimer = 64 - (value & 0x3F);
    break;
  case 0xFF17:
    ch2.nrX2 = value;
    break;
  case 0xFF18:
    ch2.nrX3 = value;
    ch2.timer = (ch2.timer & 0x700) | value;
    break;
  case 0xFF19:
    ch2.nrX4 = value;
    ch2.timer = (ch2.timer & 0xFF) | ((value & 0x07) << 8);
    if (value & 0x80) {
      ch2.enabled = true;
      if (ch2.lengthTimer == 0)
        ch2.lengthTimer = 64;
    }
    break;

  // Channel 3
  case 0xFF1A:
    ch3.nr30 = value;
    break;
  case 0xFF1B:
    ch3.nr31 = value;
    ch3.lengthTimer = 256 - value;
    break;
  case 0xFF1C:
    ch3.nr32 = value;
    break;
  case 0xFF1D:
    ch3.nr33 = value;
    ch3.timer = (ch3.timer & 0x700) | value;
    break;
  case 0xFF1E:
    ch3.nr34 = value;
    ch3.timer = (ch3.timer & 0xFF) | ((value & 0x07) << 8);
    if (value & 0x80) {
      ch3.enabled = true;
      if (ch3.lengthTimer == 0)
        ch3.lengthTimer = 256;
    }
    break;

  // Channel 4
  case 0xFF20:
    ch4.nr41 = value;
    ch4.lengthTimer = 64 - (value & 0x3F);
    break;
  case 0xFF21:
    ch4.nr42 = value;
    break;
  case 0xFF22:
    ch4.nr43 = value;
    break;
  case 0xFF23:
    ch4.nr44 = value;
    if (value & 0x80) {
      ch4.enabled = true;
      if (ch4.lengthTimer == 0)
        ch4.lengthTimer = 64;
    }
    break;

  // Control
  case 0xFF24:
    nr50 = value;
    break;
  case 0xFF25:
    nr51 = value;
    break;
  case 0xFF26:
    nr52 = (value & 0x80) | (nr52 & 0x7F);
    if (!(nr52 & 0x80)) {
      // Power off - clear all registers
      ch1.enabled = ch2.enabled = ch3.enabled = ch4.enabled = false;
      // ... would clear registers FF10-FF25 too ...
    }
    break;
  }
}

void APU::tick(int tCycles) {
  if (!(nr52 & 0x80))
    return;

  frameSequencerCounter += tCycles;
  if (frameSequencerCounter >= 8192) {
    frameSequencerCounter -= 8192;
    tickFrameSequencer();
  }

  // Channel ticking: decrease frequency timers
  // ch1
  if (ch1.enabled && --ch1.timer <= 0) {
    ch1.timer = (2048 - (((ch1.nrX4 & 0x07) << 8) | ch1.nrX3)) * 4;
    ch1.dutyStep = (ch1.dutyStep + 1) & 7;
  }
  // ch2
  if (ch2.enabled && --ch2.timer <= 0) {
    ch2.timer = (2048 - (((ch2.nrX4 & 0x07) << 8) | ch2.nrX3)) * 4;
    ch2.dutyStep = (ch2.dutyStep + 1) & 7;
  }
  // ch4 (Noise)
  if (ch4.enabled && --ch4.timer <= 0) {
    int divisor = (ch4.nr43 & 0x07);
    int divisors[] = {8, 16, 32, 48, 64, 80, 96, 112};
    int shift = (ch4.nr43 >> 4);
    ch4.timer = divisors[divisor] << shift;

    uint16_t res = (ch4.lfsr & 1) ^ ((ch4.lfsr >> 1) & 1);
    ch4.lfsr = (ch4.lfsr >> 1) | (res << 14);
    if (ch4.nr43 & 0x08) { // 7-bit mode
      ch4.lfsr = (ch4.lfsr & ~0x40) | (res << 6);
    }
  }
}

void APU::tickFrameSequencer() {
  frameSequencerStep = (frameSequencerStep + 1) & 7;

  bool clockLength = (frameSequencerStep % 2 == 0);
  bool clockEnvelope = (frameSequencerStep == 7);
  bool clockSweep = (frameSequencerStep == 2 || frameSequencerStep == 6);

  if (clockLength) {
    if (ch1.enabled && (ch1.nrX4 & 0x40)) {
      if (ch1.lengthTimer > 0) {
        if (--ch1.lengthTimer == 0)
          ch1.enabled = false;
      }
    }
    if (ch2.enabled && (ch2.nrX4 & 0x40)) {
      if (ch2.lengthTimer > 0) {
        if (--ch2.lengthTimer == 0)
          ch2.enabled = false;
      }
    }
    if (ch3.enabled && (ch3.nr34 & 0x40)) {
      if (ch3.lengthTimer > 0) {
        if (--ch3.lengthTimer == 0)
          ch3.enabled = false;
      }
    }
    if (ch4.enabled && (ch4.nr44 & 0x40)) {
      if (ch4.lengthTimer > 0) {
        if (--ch4.lengthTimer == 0)
          ch4.enabled = false;
      }
    }
  }

  if (clockSweep && ch1.sweepEnabled) {
    if (--ch1.sweepTimer <= 0) {
      int sweepPeriod = (ch1.nrX0 >> 4) & 0x07;
      ch1.sweepTimer = sweepPeriod == 0 ? 8 : sweepPeriod;

      if (sweepPeriod != 0) {
        uint16_t newFreq = calculateSweep();
        if (newFreq < 2048 && (ch1.nrX0 & 0x07) != 0) {
          ch1.shadowFrequency = newFreq;
          ch1.nrX3 = newFreq & 0xFF;
          ch1.nrX4 = (ch1.nrX4 & 0xF8) | (newFreq >> 8);
          calculateSweep(); // Check again
        }
      }
    }
  }

  if (clockEnvelope) {
    auto tickEnvelope = [](PulseChannel &ch) {
      if (ch.enabled && (ch.nrX2 & 0x07) != 0) {
        if (--ch.envelopeTimer <= 0) {
          ch.envelopeTimer = ch.nrX2 & 0x07;
          if (ch.nrX2 & 0x08) { // Increase
            if (ch.envelopeVolume < 15)
              ch.envelopeVolume++;
          } else { // Decrease
            if (ch.envelopeVolume > 0)
              ch.envelopeVolume--;
          }
        }
      }
    };
    tickEnvelope(ch1);
    tickEnvelope(ch2);
    // ... ch4 ...
  }
}

uint16_t APU::calculateSweep() {
  uint16_t newFreq = ch1.shadowFrequency >> (ch1.nrX0 & 0x07);
  if (ch1.nrX0 & 0x08) { // Decrease
    newFreq = ch1.shadowFrequency - newFreq;
  } else { // Increase
    newFreq = ch1.shadowFrequency + newFreq;
  }

  if (newFreq > 2047) {
    ch1.enabled = false;
  }
  return newFreq;
}
