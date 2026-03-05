#pragma once

#include <array>
#include <cstdint>
#include <iostream>

// Forward-declare so APU.h doesn't drag in the full AudioBackend header
// in every translation unit.
class AudioBackend;

class APU {
public:
  APU();
  ~APU();

  // Attach an audio backend. Must be set before tick() is first called.
  // If nullptr, the APU still simulates but produces no audio output.
  void setAudioBackend(AudioBackend *backend);

  uint8_t readReg(uint16_t address) const;
  void writeReg(uint16_t address, uint8_t value);

  void tick(int tCycles);
  void tickFrameSequencer();

  void serialize(std::ostream &out) const;
  void deserialize(std::istream &in);

private:
  uint16_t calculateSweep();

  // ── Frame sequencer ────────────────────────────────────────────────────────
  int frameSequencerCounter = 0;
  int frameSequencerStep = 0;

  // ── CH1/CH2 – Pulse channels ───────────────────────────────────────────────
  struct PulseChannel {
    uint8_t nrX0 = 0, nrX1 = 0, nrX2 = 0, nrX3 = 0, nrX4 = 0;
    bool enabled = false;
    int timer = 0;
    int lengthTimer = 0;
    int dutyStep = 0;
    int envelopeTimer = 0;
    int envelopeVolume = 0;
    // CH1 sweep
    int sweepTimer = 0;
    uint16_t shadowFrequency = 0;
    bool sweepEnabled = false;
  };

  // ── CH3 – Wave channel ─────────────────────────────────────────────────────
  struct WaveChannel {
    uint8_t nr30 = 0, nr31 = 0, nr32 = 0, nr33 = 0, nr34 = 0;
    uint8_t waveRAM[16]{};
    bool enabled = false;
    int timer = 0;
    int lengthTimer = 0;
    int waveStep = 0;
  };

  // ── CH4 – Noise channel ────────────────────────────────────────────────────
  struct NoiseChannel {
    uint8_t nr41 = 0, nr42 = 0, nr43 = 0, nr44 = 0;
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

  uint8_t nr50 = 0;
  uint8_t nr51 = 0;
  uint8_t nr52 = 0x80;

  // ── Sample generation ──────────────────────────────────────────────────────
  // GB CPU: 4,194,304 Hz  →  one sample every ~95.1 T-cycles at 44100 Hz.
  static constexpr double kCpuHz = 4194304.0;
  static constexpr double kSampleRateHz = 44100.0;
  // Accumulator tracks fractional progress towards the next output sample.
  double sampleAccumulator = 0.0;

  // Helper: current amplitude (0-15) for each channel
  int ch1Amplitude() const;
  int ch2Amplitude() const;
  int ch3Amplitude() const;
  int ch4Amplitude() const;

  // Mix all channels → push a single stereo int16 sample to backend
  void emitSample();

  AudioBackend *audioBackend = nullptr;
};
