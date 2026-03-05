#pragma once

#include <array>
#include <atomic>
#include <cstdint>

// Forward declaration to avoid including the full miniaudio header here.
struct ma_device;

// ---------------------------------------------------------------------------
// RingBuffer – single-producer / single-consumer, lock-free
//   - The emulator thread writes stereo int16 sample pairs.
//   - The audio callback (miniaudio thread) reads them.
// ---------------------------------------------------------------------------
class RingBuffer {
public:
  static constexpr size_t kCapacity = 8192; // must be power-of-two

  RingBuffer() : writeIdx_(0), readIdx_(0) {}

  // Push one stereo sample (left, right). Returns false if full.
  bool push(int16_t left, int16_t right) {
    size_t w = writeIdx_.load(std::memory_order_relaxed);
    size_t next = (w + 1) & kMask;
    if (next == readIdx_.load(std::memory_order_acquire))
      return false; // full
    buf_[w] = {left, right};
    writeIdx_.store(next, std::memory_order_release);
    return true;
  }

  // Pop one stereo sample. Returns false if empty.
  bool pop(int16_t &left, int16_t &right) {
    size_t r = readIdx_.load(std::memory_order_relaxed);
    if (r == writeIdx_.load(std::memory_order_acquire))
      return false; // empty
    left = buf_[r].left;
    right = buf_[r].right;
    readIdx_.store((r + 1) & kMask, std::memory_order_release);
    return true;
  }

  size_t available() const {
    size_t w = writeIdx_.load(std::memory_order_acquire);
    size_t r = readIdx_.load(std::memory_order_acquire);
    return (w - r) & kMask;
  }

private:
  static constexpr size_t kMask = kCapacity - 1;
  static_assert((kCapacity & kMask) == 0, "kCapacity must be power-of-two");

  struct Sample {
    int16_t left, right;
  };

  alignas(64) std::array<Sample, kCapacity> buf_;
  alignas(64) std::atomic<size_t> writeIdx_;
  alignas(64) std::atomic<size_t> readIdx_;
};

// ---------------------------------------------------------------------------
// AudioBackend – owns the miniaudio device and the ring buffer
// ---------------------------------------------------------------------------
class AudioBackend {
public:
  static constexpr int kSampleRate = 44100;
  static constexpr int kChannels = 2; // stereo

  AudioBackend();
  ~AudioBackend();

  // Must be called once before emulation starts. Returns false on error.
  bool init();
  void shutdown();

  // Called from the emulator thread: push one stereo sample.
  // If the buffer is full, the sample is silently dropped.
  void pushSample(int16_t left, int16_t right);

  // Exposed so the miniaudio callback can reach it.
  RingBuffer ringBuffer;

private:
  ma_device *device_ =
      nullptr; // heap-allocated to avoid including miniaudio.h here
};
