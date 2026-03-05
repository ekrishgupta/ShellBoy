#include "Timer.h"
#include "Bus.h"

Timer::Timer(Bus &b) : bus(b) {}

void Timer::tick(int cycles) {
  while (cycles > 0) {
    div_internal++;
    cycles--;

    static const int bit_map[] = {9, 3, 5, 7};
    int bit = bit_map[tac & 0x03];
    bool timer_enabled = (tac & 0x04) != 0;
    bool current_bit = timer_enabled && ((div_internal >> bit) & 0x01);

    if (last_timer_bit && !current_bit) {
      tima++;
      if (tima == 0) {
        tima = tma;
        bus.requestInterrupt(Bus::INTERRUPT_TIMER);
      }
    }
    last_timer_bit = current_bit;
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

  static const int bit_map[] = {9, 3, 5, 7};
  int bit = bit_map[tac & 0x03];
  bool timer_enabled = (tac & 0x04) != 0;
  bool current_bit = timer_enabled && ((div_internal >> bit) & 0x01);

  if (last_timer_bit && !current_bit) {
    tima++;
    if (tima == 0) {
      tima = tma;
      bus.requestInterrupt(Bus::INTERRUPT_TIMER);
    }
  }
  last_timer_bit = current_bit;
}

void Timer::serialize(std::ostream &out) const {
  out.write(reinterpret_cast<const char *>(&div_internal),
            sizeof(div_internal));
  out.write(reinterpret_cast<const char *>(&tima), sizeof(tima));
  out.write(reinterpret_cast<const char *>(&tma), sizeof(tma));
  out.write(reinterpret_cast<const char *>(&tac), sizeof(tac));
  out.write(reinterpret_cast<const char *>(&last_timer_bit),
            sizeof(last_timer_bit));
}

void Timer::deserialize(std::istream &in) {
  in.read(reinterpret_cast<char *>(&div_internal), sizeof(div_internal));
  in.read(reinterpret_cast<char *>(&tima), sizeof(tima));
  in.read(reinterpret_cast<char *>(&tma), sizeof(tma));
  in.read(reinterpret_cast<char *>(&tac), sizeof(tac));
  in.read(reinterpret_cast<char *>(&last_timer_bit), sizeof(last_timer_bit));
}
