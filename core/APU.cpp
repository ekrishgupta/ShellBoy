#include "APU.h"
#include "AudioBackend.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>

// ---------------------------------------------------------------------------
// Duty table: 4 waveforms × 8 steps
// ---------------------------------------------------------------------------
static const bool kDutyTable[4][8] = {
    {false, false, false, false, false, false, false, true}, // 12.5%
    {true, false, false, false, false, false, false, true},  // 25%
    {true, false, false, false, true, true, false, true},    // 50%
    {false, true, true, true, true, true, true, false},      // 75%
};

// ---------------------------------------------------------------------------
// APU
// ---------------------------------------------------------------------------
APU::APU() {
  nr50 = 0;
  nr51 = 0;
  nr52 = 0x80;
}

APU::~APU() {}

void APU::serialize(std::ostream &out) const {
  out.write(reinterpret_cast<const char *>(&frameSequencerCounter),
            sizeof(frameSequencerCounter));
  out.write(reinterpret_cast<const char *>(&frameSequencerStep),
            sizeof(frameSequencerStep));
  out.write(reinterpret_cast<const char *>(&ch1), sizeof(ch1));
  out.write(reinterpret_cast<const char *>(&ch2), sizeof(ch2));
  out.write(reinterpret_cast<const char *>(&ch3), sizeof(ch3));
  out.write(reinterpret_cast<const char *>(&ch4), sizeof(ch4));
  out.write(reinterpret_cast<const char *>(&nr50), sizeof(nr50));
  out.write(reinterpret_cast<const char *>(&nr51), sizeof(nr51));
  out.write(reinterpret_cast<const char *>(&nr52), sizeof(nr52));
  out.write(reinterpret_cast<const char *>(&sampleAccumulator),
            sizeof(sampleAccumulator));
}

void APU::deserialize(std::istream &in) {
  in.read(reinterpret_cast<char *>(&frameSequencerCounter),
          sizeof(frameSequencerCounter));
  in.read(reinterpret_cast<char *>(&frameSequencerStep),
          sizeof(frameSequencerStep));
  in.read(reinterpret_cast<char *>(&ch1), sizeof(ch1));
  in.read(reinterpret_cast<char *>(&ch2), sizeof(ch2));
  in.read(reinterpret_cast<char *>(&ch3), sizeof(ch3));
  in.read(reinterpret_cast<char *>(&ch4), sizeof(ch4));
  in.read(reinterpret_cast<char *>(&nr50), sizeof(nr50));
  in.read(reinterpret_cast<char *>(&nr51), sizeof(nr51));
  in.read(reinterpret_cast<char *>(&nr52), sizeof(nr52));
  in.read(reinterpret_cast<char *>(&sampleAccumulator),
          sizeof(sampleAccumulator));
}

void APU::setAudioBackend(AudioBackend *backend) { audioBackend = backend; }

// ---------------------------------------------------------------------------
// Register access
// ---------------------------------------------------------------------------
uint8_t APU::readReg(uint16_t address) const {
  if (address >= 0xFF30 && address <= 0xFF3F)
    return ch3.waveRAM[address - 0xFF30];

  switch (address) {
  // CH1
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
  // CH2
  case 0xFF16:
    return ch2.nrX1 | 0x3F;
  case 0xFF17:
    return ch2.nrX2;
  case 0xFF18:
    return 0xFF;
  case 0xFF19:
    return ch2.nrX4 | 0xBF;
  // CH3
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
  // CH4
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
  if (!(nr52 & 0x80) && address != 0xFF26)
    return;

  if (address >= 0xFF30 && address <= 0xFF3F) {
    ch3.waveRAM[address - 0xFF30] = value;
    return;
  }

  switch (address) {
  // ── CH1 ─────────────────────────────────────────────────────────────────
  case 0xFF10:
    ch1.nrX0 = value;
    break;
  case 0xFF11:
    ch1.nrX1 = value;
    ch1.lengthTimer = 64 - (value & 0x3F);
    break;
  case 0xFF12:
    ch1.nrX2 = value;
    if ((value >> 3) == 0)
      ch1.enabled = false; // DAC off
    break;
  case 0xFF13:
    ch1.nrX3 = value;
    break;
  case 0xFF14:
    ch1.nrX4 = value;
    if (value & 0x80) { // Trigger
      ch1.enabled = true;
      if (ch1.lengthTimer == 0)
        ch1.lengthTimer = 64;
      ch1.timer = (2048 - (((ch1.nrX4 & 0x07) << 8) | ch1.nrX3)) * 4;
      ch1.envelopeVolume = (ch1.nrX2 >> 4);
      ch1.envelopeTimer = (ch1.nrX2 & 0x07);
      // Sweep
      ch1.shadowFrequency = ((ch1.nrX4 & 0x07) << 8) | ch1.nrX3;
      int sweepPeriod = (ch1.nrX0 >> 4) & 0x07;
      int sweepShift = ch1.nrX0 & 0x07;
      ch1.sweepTimer = (sweepPeriod == 0) ? 8 : sweepPeriod;
      ch1.sweepEnabled = (sweepPeriod != 0 || sweepShift != 0);
      if (sweepShift != 0)
        calculateSweep();
      if ((ch1.nrX2 >> 3) == 0)
        ch1.enabled = false; // DAC off
    }
    break;

  // ── CH2 ─────────────────────────────────────────────────────────────────
  case 0xFF16:
    ch2.nrX1 = value;
    ch2.lengthTimer = 64 - (value & 0x3F);
    break;
  case 0xFF17:
    ch2.nrX2 = value;
    if ((value >> 3) == 0)
      ch2.enabled = false;
    break;
  case 0xFF18:
    ch2.nrX3 = value;
    break;
  case 0xFF19:
    ch2.nrX4 = value;
    if (value & 0x80) {
      ch2.enabled = true;
      if (ch2.lengthTimer == 0)
        ch2.lengthTimer = 64;
      ch2.timer = (2048 - (((ch2.nrX4 & 0x07) << 8) | ch2.nrX3)) * 4;
      ch2.envelopeVolume = (ch2.nrX2 >> 4);
      ch2.envelopeTimer = (ch2.nrX2 & 0x07);
      if ((ch2.nrX2 >> 3) == 0)
        ch2.enabled = false;
    }
    break;

  // ── CH3 ─────────────────────────────────────────────────────────────────
  case 0xFF1A:
    ch3.nr30 = value;
    if (!(value & 0x80))
      ch3.enabled = false;
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
    break;
  case 0xFF1E:
    ch3.nr34 = value;
    if (value & 0x80) {
      ch3.enabled = true;
      if (ch3.lengthTimer == 0)
        ch3.lengthTimer = 256;
      ch3.timer = (2048 - (((ch3.nr34 & 0x07) << 8) | ch3.nr33)) * 2;
      ch3.waveStep = 0;
      if (!(ch3.nr30 & 0x80))
        ch3.enabled = false;
    }
    break;

  // ── CH4 ─────────────────────────────────────────────────────────────────
  case 0xFF20:
    ch4.nr41 = value;
    ch4.lengthTimer = 64 - (value & 0x3F);
    break;
  case 0xFF21:
    ch4.nr42 = value;
    if ((value >> 3) == 0)
      ch4.enabled = false;
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
      ch4.lfsr = 0x7FFF;
      ch4.envelopeVolume = (ch4.nr42 >> 4);
      ch4.envelopeTimer = (ch4.nr42 & 0x07);
      if ((ch4.nr42 >> 3) == 0)
        ch4.enabled = false;
    }
    break;

  // ── Control ─────────────────────────────────────────────────────────────
  case 0xFF24:
    nr50 = value;
    break;
  case 0xFF25:
    nr51 = value;
    break;
  case 0xFF26:
    nr52 = (value & 0x80) | (nr52 & 0x7F);
    if (!(nr52 & 0x80)) {
      ch1 = PulseChannel{};
      ch2 = PulseChannel{};
      ch3 = WaveChannel{};
      ch4 = NoiseChannel{};
    }
    break;
  }
}

// ---------------------------------------------------------------------------
// Tick – called once per CPU instruction, tCycles = M-cycles * 4
// ---------------------------------------------------------------------------
void APU::tick(int tCycles) {
  if (!(nr52 & 0x80))
    return;

  // ── Frame sequencer ──────────────────────────────────────────────────────
  frameSequencerCounter += tCycles;
  if (frameSequencerCounter >= 8192) {
    frameSequencerCounter -= 8192;
    tickFrameSequencer();
  }

  // ── CH1 frequency timer ──────────────────────────────────────────────────
  if (ch1.enabled) {
    ch1.timer -= tCycles;
    while (ch1.timer <= 0) {
      int period = (2048 - (((ch1.nrX4 & 0x07) << 8) | ch1.nrX3)) * 4;
      ch1.timer += (period > 0 ? period : 1);
      ch1.dutyStep = (ch1.dutyStep + 1) & 7;
    }
  }

  // ── CH2 frequency timer ──────────────────────────────────────────────────
  if (ch2.enabled) {
    ch2.timer -= tCycles;
    while (ch2.timer <= 0) {
      int period = (2048 - (((ch2.nrX4 & 0x07) << 8) | ch2.nrX3)) * 4;
      ch2.timer += (period > 0 ? period : 1);
      ch2.dutyStep = (ch2.dutyStep + 1) & 7;
    }
  }

  // ── CH3 wave timer ───────────────────────────────────────────────────────
  if (ch3.enabled) {
    ch3.timer -= tCycles;
    while (ch3.timer <= 0) {
      int period = (2048 - (((ch3.nr34 & 0x07) << 8) | ch3.nr33)) * 2;
      ch3.timer += (period > 0 ? period : 1);
      ch3.waveStep = (ch3.waveStep + 1) & 31;
    }
  }

  // ── CH4 noise timer ──────────────────────────────────────────────────────
  if (ch4.enabled) {
    ch4.timer -= tCycles;
    while (ch4.timer <= 0) {
      static const int kDivisors[8] = {8, 16, 32, 48, 64, 80, 96, 112};
      int divisor = ch4.nr43 & 0x07;
      int shift = (ch4.nr43 >> 4) & 0x0F;
      int period = kDivisors[divisor] << shift;
      ch4.timer += (period > 0 ? period : 1);

      uint16_t res = (ch4.lfsr & 1) ^ ((ch4.lfsr >> 1) & 1);
      ch4.lfsr = (ch4.lfsr >> 1) | (res << 14);
      if (ch4.nr43 & 0x08) { // 7-bit mode
        ch4.lfsr = (ch4.lfsr & ~0x0040) | (res << 6);
      }
    }
  }

  // ── Sample generation ────────────────────────────────────────────────────
  // We accumulate T-cycles; every ~95.1 T-cycles we emit one stereo sample.
  sampleAccumulator += tCycles;
  while (sampleAccumulator >= (kCpuHz / kSampleRateHz)) {
    sampleAccumulator -= (kCpuHz / kSampleRateHz);
    emitSample();
  }
}

// ---------------------------------------------------------------------------
// Frame sequencer
// ---------------------------------------------------------------------------
void APU::tickFrameSequencer() {
  frameSequencerStep = (frameSequencerStep + 1) & 7;

  bool clockLength = (frameSequencerStep % 2 == 0);
  bool clockSweep = (frameSequencerStep == 2 || frameSequencerStep == 6);
  bool clockEnvelope = (frameSequencerStep == 7);

  // Length counters
  if (clockLength) {
    auto tickLen = [](auto &ch, int max, uint8_t &nrX4flag) {
      if (ch.enabled && (nrX4flag & 0x40)) {
        if (ch.lengthTimer > 0 && --ch.lengthTimer == 0)
          ch.enabled = false;
      }
    };
    tickLen(ch1, 64, ch1.nrX4);
    tickLen(ch2, 64, ch2.nrX4);
    if (ch3.enabled && (ch3.nr34 & 0x40)) {
      if (ch3.lengthTimer > 0 && --ch3.lengthTimer == 0)
        ch3.enabled = false;
    }
    if (ch4.enabled && (ch4.nr44 & 0x40)) {
      if (ch4.lengthTimer > 0 && --ch4.lengthTimer == 0)
        ch4.enabled = false;
    }
  }

  // Sweep (CH1 only)
  if (clockSweep && ch1.sweepEnabled) {
    if (--ch1.sweepTimer <= 0) {
      int sweepPeriod = (ch1.nrX0 >> 4) & 0x07;
      ch1.sweepTimer = (sweepPeriod == 0) ? 8 : sweepPeriod;
      if (sweepPeriod != 0) {
        uint16_t newFreq = calculateSweep();
        if (newFreq < 2048 && (ch1.nrX0 & 0x07) != 0) {
          ch1.shadowFrequency = newFreq;
          ch1.nrX3 = newFreq & 0xFF;
          ch1.nrX4 = (ch1.nrX4 & 0xF8) | (newFreq >> 8);
          calculateSweep(); // second check for overflow
        }
      }
    }
  }

  // Envelope (CH1, CH2, CH4)
  if (clockEnvelope) {
    auto tickEnv = [](auto &ch) {
      int period = ch.nrX2 & 0x07;
      if (ch.enabled && period != 0) {
        if (--ch.envelopeTimer <= 0) {
          ch.envelopeTimer = period;
          if (ch.nrX2 & 0x08) {
            if (ch.envelopeVolume < 15)
              ch.envelopeVolume++;
          } else {
            if (ch.envelopeVolume > 0)
              ch.envelopeVolume--;
          }
        }
      }
    };
    tickEnv(ch1);
    tickEnv(ch2);
    // CH4 envelope
    {
      int period = ch4.nr42 & 0x07;
      if (ch4.enabled && period != 0) {
        if (--ch4.envelopeTimer <= 0) {
          ch4.envelopeTimer = period;
          if (ch4.nr42 & 0x08) {
            if (ch4.envelopeVolume < 15)
              ch4.envelopeVolume++;
          } else {
            if (ch4.envelopeVolume > 0)
              ch4.envelopeVolume--;
          }
        }
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Sweep helper (CH1)
// ---------------------------------------------------------------------------
uint16_t APU::calculateSweep() {
  uint16_t newFreq = ch1.shadowFrequency >> (ch1.nrX0 & 0x07);
  if (ch1.nrX0 & 0x08)
    newFreq = ch1.shadowFrequency - newFreq;
  else
    newFreq = ch1.shadowFrequency + newFreq;

  if (newFreq > 2047)
    ch1.enabled = false;
  return newFreq;
}

// ---------------------------------------------------------------------------
// Amplitude helpers (0-15)
// ---------------------------------------------------------------------------
int APU::ch1Amplitude() const {
  if (!ch1.enabled)
    return 0;
  int duty = (ch1.nrX1 >> 6) & 0x03;
  return kDutyTable[duty][ch1.dutyStep] ? ch1.envelopeVolume : 0;
}

int APU::ch2Amplitude() const {
  if (!ch2.enabled)
    return 0;
  int duty = (ch2.nrX1 >> 6) & 0x03;
  return kDutyTable[duty][ch2.dutyStep] ? ch2.envelopeVolume : 0;
}

int APU::ch3Amplitude() const {
  if (!ch3.enabled)
    return 0;
  // Volume shift from NR32 bits 5-6: 0=mute, 1=100%, 2=50%, 3=25%
  int volShift = (ch3.nr32 >> 5) & 0x03;
  if (volShift == 0)
    return 0;

  // Read nibble from wave RAM
  int byteIndex = ch3.waveStep >> 1;
  uint8_t byte = ch3.waveRAM[byteIndex];
  int nibble = (ch3.waveStep & 1) ? (byte & 0x0F) : (byte >> 4);

  // Scale: 100% → no shift, 50% → >>1, 25% → >>2
  return nibble >> (volShift - 1);
}

int APU::ch4Amplitude() const {
  if (!ch4.enabled)
    return 0;
  // LFSR bit 0 inverted gives 1 (high) or 0 (low)
  return (!(ch4.lfsr & 1)) ? ch4.envelopeVolume : 0;
}

// ---------------------------------------------------------------------------
// emitSample – mix all channels and push to the audio backend
// ---------------------------------------------------------------------------
void APU::emitSample() {
  if (!audioBackend)
    return;

  const int a1 = ch1Amplitude();
  const int a2 = ch2Amplitude();
  const int a3 = ch3Amplitude();
  const int a4 = ch4Amplitude();

  int leftMix = 0;
  int rightMix = 0;

  if (nr51 & 0x80)
    leftMix += a4;
  if (nr51 & 0x40)
    leftMix += a3;
  if (nr51 & 0x20)
    leftMix += a2;
  if (nr51 & 0x10)
    leftMix += a1;
  if (nr51 & 0x08)
    rightMix += a4;
  if (nr51 & 0x04)
    rightMix += a3;
  if (nr51 & 0x02)
    rightMix += a2;
  if (nr51 & 0x01)
    rightMix += a1;

  int leftVol = (nr50 >> 4) & 0x07;
  int rightVol = (nr50) & 0x07;

  constexpr float kMax = 60.0f * 8.0f;
  auto toInt16 = [&](int mix, int vol) -> int16_t {
    float sample = static_cast<float>(mix * (vol + 1)) / kMax;
    sample = (sample - 0.5f) * 2.0f;
    return static_cast<int16_t>(
        std::clamp(static_cast<int>(sample * 32767.0f), -32767, 32767));
  };

  audioBackend->pushSample(toInt16(leftMix, leftVol),
                           toInt16(rightMix, rightVol));
}
