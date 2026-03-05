#include "core/APU.h"
#include "core/AudioBackend.h"
#include "core/Bus.h"
#include "core/CPU.h"
#include "core/Joypad.h"
#include "core/PPU.h"
#include "core/Serial.h"
#include "core/Timer.h"
#include "frontend/BrailleRenderer.h"
#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
#include "mmu/Cartridge.h"
#include <atomic>
#include <chrono>
#include <fstream>
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
  Joypad joypad(bus);
  Serial serial(&bus);
  APU apu;

  // ── Audio backend ──────────────────────────────────────────────────────────
  AudioBackend audioBackend;
  if (!audioBackend.init()) {
    std::cerr << "[Warning] Audio initialisation failed – running silently.\n";
  }
  apu.setAudioBackend(&audioBackend);

  bus.setPPU(&ppu);
  bus.setTimer(&timer);
  bus.setJoypad(&joypad);
  bus.setSerial(&serial);
  bus.setAPU(&apu);

  BrailleRenderer renderer;
  auto screen = ScreenInteractive::TerminalOutput();

  std::atomic<int> frames = 0;
  const std::string saveStatePath = "save.sst";

  auto renderer_component = Renderer([&] {
    std::string frameText = renderer.render(ppu.frameBuffer);
    return window(
        text("ShellBoy - DMG-01 Emulator"),
        vbox({text("Frames: " + std::to_string(frames.load())),
              text("Controls: Arrows=D-Pad, Z=A, X=B, Enter=Start, "
                   "Backspace=Select"),
              text("State: S=Save, L=Load"), separator(), text(frameText)}));
  });

  renderer_component |= CatchEvent([&](Event event) {
    if (event == Event::ArrowUp) {
      joypad.pressButton(Joypad::UP);
      return true;
    }
    if (event == Event::ArrowDown) {
      joypad.pressButton(Joypad::DOWN);
      return true;
    }
    if (event == Event::ArrowLeft) {
      joypad.pressButton(Joypad::LEFT);
      return true;
    }
    if (event == Event::ArrowRight) {
      joypad.pressButton(Joypad::RIGHT);
      return true;
    }
    if (event == Event::Character("z") || event == Event::Character("Z")) {
      joypad.pressButton(Joypad::A);
      return true;
    }
    if (event == Event::Character("x") || event == Event::Character("X")) {
      joypad.pressButton(Joypad::B);
      return true;
    }
    if (event == Event::Return) {
      joypad.pressButton(Joypad::START);
      return true;
    }
    if (event == Event::Backspace) {
      joypad.pressButton(Joypad::SELECT);
      return true;
    }
    if (event == Event::Character("q") || event == Event::Character("Q")) {
      screen.Exit();
      return true;
    }
    if (event == Event::Character("s") || event == Event::Character("S")) {
      std::ofstream out(saveStatePath, std::ios::binary);
      if (out.is_open()) {
        bus.serialize(out);
        cpu.serialize(out);
        ppu.serialize(out);
        timer.serialize(out);
        joypad.serialize(out);
        apu.serialize(out);
        cart.serialize(out);
        std::cout << "State saved to " << saveStatePath << std::endl;
      } else {
        std::cerr << "Failed to save state: " << saveStatePath << std::endl;
      }
      return true;
    }
    if (event == Event::Character("l") || event == Event::Character("L")) {
      std::ifstream in(saveStatePath, std::ios::binary);
      if (in.is_open()) {
        bus.deserialize(in);
        cpu.deserialize(in);
        ppu.deserialize(in);
        timer.deserialize(in);
        joypad.deserialize(in);
        apu.deserialize(in);
        cart.deserialize(in);
        std::cout << "State loaded from " << saveStatePath << std::endl;
      } else {
        std::cerr << "Failed to load state: " << saveStatePath << std::endl;
      }
      return true;
    }
    return false;
  });

  std::atomic<bool> running = true;

  // ── Master clock loop ──────────────────────────────────────────────────────
  std::thread emulatorThread([&]() {
    const double targetFPS = 60.7;
    const auto frameDuration = std::chrono::duration<double>(1.0 / targetFPS);

    while (running) {
      auto frameStart = std::chrono::high_resolution_clock::now();

      // Release all buttons (TUI only keeps press for one frame)
      joypad.releaseButton(Joypad::UP);
      joypad.releaseButton(Joypad::DOWN);
      joypad.releaseButton(Joypad::LEFT);
      joypad.releaseButton(Joypad::RIGHT);
      joypad.releaseButton(Joypad::A);
      joypad.releaseButton(Joypad::B);
      joypad.releaseButton(Joypad::SELECT);
      joypad.releaseButton(Joypad::START);

      // Run until one full frame (70224 T-cycles) is generated.
      int cyclesThisFrame = 0;
      while (cyclesThisFrame < 70224) {
        int cycles = cpu.tick();
        bus.tick(cycles);
        timer.tick(cycles);
        serial.tick(cycles);
        apu.tick(cycles); // ← APU now produces samples
        for (int i = 0; i < cycles; ++i)
          ppu.tick();
        cyclesThisFrame += cycles;
      }

      ppu.frameReady = false;
      frames++;

      screen.PostEvent(Event::Custom);

      auto frameEnd = std::chrono::high_resolution_clock::now();
      auto elapsed = frameEnd - frameStart;
      if (elapsed < frameDuration)
        std::this_thread::sleep_for(frameDuration - elapsed);
    }
  });

  screen.Loop(renderer_component);

  running = false;
  emulatorThread.join();

  // AudioBackend destructor handles shutdown.
  return 0;
}
