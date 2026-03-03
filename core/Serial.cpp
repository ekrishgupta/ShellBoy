#include "Serial.h"
#include "Bus.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

Serial::Serial(Bus *bus) : bus(bus) { initNetwork(); }

Serial::~Serial() {
  quit = true;
  if (net_thread.joinable())
    net_thread.join();
  if (server_fd != -1)
    close(server_fd);
  if (client_fd != -1)
    close(client_fd);
}

uint8_t Serial::read(uint16_t address) const {
  if (address == 0xFF01) {
    return sb;
  } else if (address == 0xFF02) {
    return sc | 0x7E; // Bits 1-6 are usually 1
  }
  return 0xFF;
}

void Serial::write(uint16_t address, uint8_t value) {
  if (address == 0xFF01) {
    sb = value;
  } else if (address == 0xFF02) {
    sc = value;
    // Bit 7: Transfer Start
    // Bit 0: Shift Clock (1 = Internal, 0 = External)
    if ((sc & 0x80) && (sc & 0x01)) {
      transferring = true;
      transfer_cycles = 0;

      // For now, just print to stdout if it's a test rom
      // Blargg's test ROMs use this
      // std::cout << (char)sb << std::flush;
    }
  }
}

void Serial::tick(uint32_t cycles) {
  if (!transferring)
    return;

  transfer_cycles += cycles;

  // Serial transfer speed is 8192Hz (one bit every 128 cycles, one byte every
  // 1024 cycles)
  if (transfer_cycles >= 1024) {
    // Transfer complete
    transferring = false;
    sc &= 0x7F; // Clear transfer start bit

    // Print to stdout (Blargg convention)
    std::cout << (char)sb << std::flush;

    // Request Serial interrupt
    if (bus) {
      bus->requestInterrupt(Bus::INTERRUPT_SERIAL);
    }
  }
}
