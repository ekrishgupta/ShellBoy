// MinAudio implementation – compiled once here.
#define MINIAUDIO_IMPLEMENTATION
#include "../thirdparty/miniaudio.h"

#include "AudioBackend.h"

#include <cstring>
#include <iostream>

// ---------------------------------------------------------------------------
// miniaudio data callback – runs on the audio thread.
// Drains the ring buffer into the PCM output; fills silence if starved.
// ---------------------------------------------------------------------------
static void audioDataCallback(ma_device *pDevice, void *pOutput, const void *,
                              ma_uint32 frameCount) {
  auto *backend = reinterpret_cast<AudioBackend *>(pDevice->pUserData);
  auto *out = reinterpret_cast<int16_t *>(pOutput);

  for (ma_uint32 i = 0; i < frameCount; ++i) {
    int16_t left = 0;
    int16_t right = 0;
    backend->ringBuffer.pop(left, right);
    out[i * 2 + 0] = left;
    out[i * 2 + 1] = right;
  }
}

// ---------------------------------------------------------------------------
// AudioBackend
// ---------------------------------------------------------------------------
AudioBackend::AudioBackend() = default;

AudioBackend::~AudioBackend() { shutdown(); }

bool AudioBackend::init() {
  device_ = new ma_device;

  ma_device_config config = ma_device_config_init(ma_device_type_playback);
  config.playback.format = ma_format_s16;
  config.playback.channels = static_cast<ma_uint32>(kChannels);
  config.sampleRate = static_cast<ma_uint32>(kSampleRate);
  config.dataCallback = audioDataCallback;
  config.pUserData = this;
  // Keep the buffer small to reduce latency (but not so small that
  // we underrun constantly). 512 frames ≈ 11 ms at 44100 Hz.
  config.periodSizeInFrames = 512;

  if (ma_device_init(nullptr, &config, device_) != MA_SUCCESS) {
    std::cerr << "[AudioBackend] Failed to initialise miniaudio device.\n";
    delete device_;
    device_ = nullptr;
    return false;
  }

  if (ma_device_start(device_) != MA_SUCCESS) {
    std::cerr << "[AudioBackend] Failed to start miniaudio device.\n";
    ma_device_uninit(device_);
    delete device_;
    device_ = nullptr;
    return false;
  }

  std::cout << "[AudioBackend] Playback started: " << kSampleRate
            << " Hz, stereo, 16-bit PCM.\n";
  return true;
}

void AudioBackend::shutdown() {
  if (device_) {
    ma_device_stop(device_);
    ma_device_uninit(device_);
    delete device_;
    device_ = nullptr;
  }
}

void AudioBackend::pushSample(int16_t left, int16_t right) {
  // Drop silently if buffer is full – prevents emulator from blocking
  ringBuffer.push(left, right);
}
