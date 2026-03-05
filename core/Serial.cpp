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
    if (sc & 0x80) {
      // Bit 0: Shift Clock (1 = Internal, 0 = External)
      if (sc & 0x01) {
        transferring = true;
        transfer_cycles = 0;

        // If connected, send the byte
        std::lock_guard<std::mutex> lock(net_mutex);
        if (is_connected) {
          ::write(client_fd, &sb, 1);
        }
      } else {
        // External clock - wait for incoming byte
        transferring = true;
        transfer_cycles = 0;
      }
    }
  }
}

void Serial::tick(uint32_t cycles) {
  if (!transferring)
    return;

  bool finished = false;
  uint8_t next_sb = 0xFF;

  if (sc & 0x01) { // Internal Clock (Master)
    transfer_cycles += cycles;
    if (transfer_cycles >= 1024) {
      finished = true;
      std::lock_guard<std::mutex> lock(net_mutex);
      if (byte_received) {
        next_sb = received_byte;
        byte_received = false;
      } else {
        next_sb = 0xFF; // Nothing received
      }
    }
  } else { // External Clock (Slave)
    std::lock_guard<std::mutex> lock(net_mutex);
    if (byte_received) {
      next_sb = received_byte;
      byte_received = false;
      finished = true;

      // When slave receives a byte, it should probably send its own SB back
      if (is_connected) {
        ::write(client_fd, &sb, 1);
      }
    }
    // Also timeout or something? Usually slaves wait indefinitely.
  }

  if (finished) {
    sb = next_sb;
    transferring = false;
    sc &= 0x7F; // Clear transfer start bit

    // Print to stdout (Blargg convention)
    if (sb != 0x00 && sb != 0xFF) {
      std::cout << (char)sb << std::flush;
    }

    // Request Serial interrupt
    if (bus) {
      bus->requestInterrupt(Bus::INTERRUPT_SERIAL);
    }
  }
}

void Serial::initNetwork() {
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1)
    return;

  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(8765);

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    // Port busy, likely another instance is server. Try connecting.
    close(server_fd);
    server_fd = -1;
    net_thread = std::thread(&Serial::connectThread, this);
  } else {
    listen(server_fd, 1);
    net_thread = std::thread(&Serial::listenThread, this);
  }
}

void Serial::listenThread() {
  struct sockaddr_in address;
  socklen_t addrlen = sizeof(address);
  while (!quit) {
    int new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
    if (new_socket >= 0) {
      {
        std::lock_guard<std::mutex> lock(net_mutex);
        client_fd = new_socket;
        is_connected = true;
      }
      // Set non-blocking
      fcntl(client_fd, F_SETFL, O_NONBLOCK);

      while (!quit && is_connected) {
        uint8_t buffer;
        int valread = ::read(client_fd, &buffer, 1);
        if (valread == 1) {
          std::lock_guard<std::mutex> lock(net_mutex);
          received_byte = buffer;
          byte_received = true;
        } else if (valread == 0) {
          std::lock_guard<std::mutex> lock(net_mutex);
          is_connected = false;
          close(client_fd);
          client_fd = -1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

void Serial::connectThread() {
  while (!quit && !is_connected) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8765);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == 0) {
      {
        std::lock_guard<std::mutex> lock(net_mutex);
        client_fd = sock;
        is_connected = true;
      }
      fcntl(client_fd, F_SETFL, O_NONBLOCK);

      while (!quit && is_connected) {
        uint8_t buffer;
        int valread = ::read(client_fd, &buffer, 1);
        if (valread == 1) {
          std::lock_guard<std::mutex> lock(net_mutex);
          received_byte = buffer;
          byte_received = true;
        } else if (valread == 0) {
          std::lock_guard<std::mutex> lock(net_mutex);
          is_connected = false;
          close(client_fd);
          client_fd = -1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    } else {
      close(sock);
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}
void Serial::serialize(std::ostream &out) const {
  out.write(reinterpret_cast<const char *>(&sb), sizeof(sb));
  out.write(reinterpret_cast<const char *>(&sc), sizeof(sc));
  out.write(reinterpret_cast<const char *>(&transferring),
            sizeof(transferring));
  out.write(reinterpret_cast<const char *>(&transfer_cycles),
            sizeof(transfer_cycles));
}

void Serial::deserialize(std::istream &in) {
  in.read(reinterpret_cast<char *>(&sb), sizeof(sb));
  in.read(reinterpret_cast<char *>(&sc), sizeof(sc));
  in.read(reinterpret_cast<char *>(&transferring), sizeof(transferring));
  in.read(reinterpret_cast<char *>(&transfer_cycles), sizeof(transfer_cycles));
}
