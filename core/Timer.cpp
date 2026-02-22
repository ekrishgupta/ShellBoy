#include "Timer.h"
#include "Bus.h"

Timer::Timer(Bus &b) : bus(b) {}

void Timer::tick(int cycles) {
  uint16_t prev_div = div_internal;
  div_internal += cycles;

  static const int bit_map[] = {9, 3, 5, 7};
  int bit = bit_map[tac & 0x03];
  bool timer_enabled = (tac & 0x04) != 0;

  if (timer_enabled) {
    bool prev_bit = (prev_div >> bit) & 0x01;
    bool current_bit = (div_internal >> bit) & 0x01;

    if (prev_bit && !current_bit) {
      tima++;
      if (tima == 0) {
        tima = tma;
        bus.requestInterrupt(Bus::INTERRUPT_TIMER);
      }
    }
  }
}

uint8_t Timer::read(uint16_t address) const {
  switch (address) {
  case 0xFF04:
    return static_cast<uint8_t>(div_internal >> 8);
  case 0xFF05:
    return tima;
  case 0xFF06:
    return tma;
  case 0xFF07:
    return tac;
  default:
    return 0xFF;
  }
}

void Timer::write(uint16_t address, uint8_t value) {
  switch (address) {
  case 0xFF04:
    div_internal = 0; // Writing any value to DIV resets it to 0
    break;
  case 0xFF05:
    tima = value;
    break;
  case 0xFF06:
    tma = value;
    break;
  case 0xFF07:
    tac = value;
    break;
  }
}
