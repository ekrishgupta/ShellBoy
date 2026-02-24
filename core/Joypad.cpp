#include "Joypad.h"
#include "Bus.h"

Joypad::Joypad(Bus &b) : bus(b) {}

Joypad::~Joypad() {}

void Joypad::pressButton(Button button) {
  uint8_t prev_buttons = buttons;
  buttons &= ~(1 << static_cast<int>(button));

  // Trigger interrupt if a button becomes pressed (transition from 1 to 0)
  // and the corresponding group is selected.
  bool bit_changed = (prev_buttons & (1 << static_cast<int>(button))) != 0;
  if (bit_changed) {
    bool is_direction = static_cast<int>(button) <= 3;
    bool is_button = static_cast<int>(button) >= 4;

    bool direction_selected = (select & 0x10) == 0;
    bool button_selected = (select & 0x20) == 0;

    if ((is_direction && direction_selected) ||
        (is_button && button_selected)) {
      bus.requestInterrupt(Bus::INTERRUPT_JOYPAD);
    }
  }
}

void Joypad::releaseButton(Button button) {
  buttons |= (1 << static_cast<int>(button));
}

uint8_t Joypad::read() const {
  uint8_t res =
      0xCF | select; // Bits 6,7 are always 1. Low 4 bits are 1 by default.

  if ((select & 0x10) == 0) { // Direction keys selected
    res &= (0xF0 | (buttons & 0x0F));
  }
  if ((select & 0x20) == 0) { // Button keys selected
    res &= (0xF0 | ((buttons >> 4) & 0x0F));
  }

  return res;
}

void Joypad::write(uint8_t value) { select = value & 0x30; }
