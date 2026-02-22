#include "core/Bus.h"
#include "core/CPU.h"
#include "core/PPU.h"
#include "core/Timer.h"
#include "frontend/BrailleRenderer.h"
#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
#include "mmu/Cartridge.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

using namespace ftxui;

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "Usage: ShellBoy <rom_path>" << std::endl;
    return 1;
  }
  Bus bus;
  Cartridge cart;
  if (!cart.loadRom(argv[1])) {
    std::cerr << "Failed to load ROM: " << argv[1] << std::endl;
    return 1;
  }

  bus.setCartridge(&cart);

  CPU cpu(bus);
  PPU ppu(bus);
  Timer timer(bus);
  bus.setPPU(&ppu);
  bus.setTimer(&timer);
  BrailleRenderer renderer;

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
        timer.tick(cycles);
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
