#pragma once

#include <array>
#include <cstdint>

class APU {
public:
  APU();
  ~APU();

  uint8_t readReg(uint16_t address) const;
  void writeReg(uint16_t address, uint8_t value);

  void tick(int tCycles);
  void tickFrameSequencer();

private:
  uint16_t calculateSweep();

  int frameSequencerCounter = 0;
  int frameSequencerStep = 0;

  struct PulseChannel {
    uint8_t nrX0 = 0, nrX1 = 0, nrX2 = 0, nrX3 = 0, nrX4 = 0;
    // Internal state
    bool enabled = false;
    float frequency = 0;
    int timer = 0;
    int lengthTimer = 0;
    int dutyStep = 0;
    int envelopeTimer = 0;
    int envelopeVolume = 0;
    // Sweep (Only for CH1)
    int sweepTimer = 0;
    uint16_t shadowFrequency = 0;
    bool sweepEnabled = false;
  };

  struct WaveChannel {
    uint8_t nr30, nr31, nr32, nr33, nr34;
    uint8_t waveRAM[16];
    // Internal state
    bool enabled = false;
    int timer = 0;
    int lengthTimer = 0;
    int waveStep = 0;
  };

  struct NoiseChannel {
    uint8_t nr41, nr42, nr43, nr44;
    // Internal state
    bool enabled = false;
    int timer = 0;
    int lengthTimer = 0;
    uint16_t lfsr = 0x7FFF;
    int envelopeTimer = 0;
    int envelopeVolume = 0;
  };

  PulseChannel ch1, ch2;
  WaveChannel ch3;
  NoiseChannel ch4;

  uint8_t nr50, nr51, nr52;
};
