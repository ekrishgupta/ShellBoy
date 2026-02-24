#pragma once

#include <cstdint>

class Bus;

class Joypad {
public:
  explicit Joypad(Bus &bus);
  ~Joypad();

  enum Button {
    RIGHT = 0,
    LEFT = 1,
    UP = 2,
    DOWN = 3,
    A = 4,
    B = 5,
    SELECT = 6,
    START = 7
  };

  void pressButton(Button button);
  void releaseButton(Button button);

  uint8_t read() const;
  void write(uint8_t value);

private:
  Bus &bus;
  uint8_t buttons = 0xFF; // 1 = Released, 0 = Pressed
  uint8_t select = 0x30;  // Bits 4 and 5
};
