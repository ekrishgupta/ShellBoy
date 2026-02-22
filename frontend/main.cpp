#include "core/Bus.h"
#include "core/CPU.h"
#include "core/PPU.h"
#include "frontend/BrailleRenderer.h"
#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
#include <chrono>
#include <iostream>
#include <thread>
#include <atomic>

using namespace ftxui;

int main() {
  Bus bus;
  CPU cpu(bus);
  PPU ppu(bus);
  BrailleRenderer renderer;

  // Simulate some visual data for testing the renderer without a ROM
  for (int y = 0; y < 144; ++y) {
    for (int x = 0; x < 160; ++x) {
      if ((x + y) % 5 == 0 || (x - y) % 7 == 0) {
        ppu.frameBuffer[y * 160 + x] = 1;
      }
    }
  }

  auto screen = ScreenInteractive::TerminalOutput();

  std::atomic<int> frames = 0;
  auto renderer_component = Renderer([&] {
    std::string frameText = renderer.render(ppu.frameBuffer);
    return window(text("ShellBoy - DMG-01 Emulator"),
                  vbox({text("Frames: " + std::to_string(frames.load())),
                        text(frameText)}));
  });

  std::atomic<bool> running = true;
  // The Master Clock Loop
  std::thread emulatorThread([&]() {
    const double targetFPS = 60.7;
    const auto frameDuration = std::chrono::duration<double>(1.0 / targetFPS);

    while (running) {
      auto frameStart = std::chrono::high_resolution_clock::now();

      // Run CPU and PPU until a frame is ready
      // A full frame is 70224 T-cycles
      int cyclesThisFrame = 0;
      while (cyclesThisFrame < 70224) {
        int cycles = cpu.tick(); // CPU is currently a stub NOP taking 4 cycles
        for (int i = 0; i < cycles; ++i) {
          ppu.tick();
        }
        cyclesThisFrame += cycles;
      }

      ppu.frameReady = false;
      frames++;

      // Trigger a UI re-render on the main thread safely
      screen.PostEvent(Event::Custom);

      // Sleep to maintain 60.7 Hz limit
      auto frameEnd = std::chrono::high_resolution_clock::now();
      auto elapsed = frameEnd - frameStart;
      if (elapsed < frameDuration) {
        std::this_thread::sleep_for(frameDuration - elapsed);
      }
    }
  });

  // Start FTXUI event loop (this blocks until the UI exits)
  screen.Loop(renderer_component);

  // Stop emulator thread gracefully
  running = false;
  emulatorThread.join();

  return 0;
}
