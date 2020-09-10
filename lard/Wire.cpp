#include "Wire.h"

#include <iostream>

#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


class syserror {
  public:
    syserror(int e_) : e(e_) { }
    int e;
};

inline std::ostream& operator<<(std::ostream& s, const syserror& se) {
  s << "[" << std::dec << se.e << ": " << strerror(se.e) << "]";
  return s;
}



TwoWire::TwoWire()
  : fd(-1), tx_next(tx_buf.begin())
  { }

TwoWire::~TwoWire() {
  end();
}

void TwoWire::begin() {
  if (fd < 0) {
    fd = open("/dev/i2c-1", O_RDWR);
    if (fd < 0) {
      auto e = errno;
      std::cerr << "i2c open failed " << syserror(e) << std::endl;
    }
  }
  tx_next = tx_buf.begin();
}

void TwoWire::end() {
  if (fd >= 0) {
    close(fd);
    fd = -1;
  }
}

void TwoWire::beginTransmission(int8_t addr) {
  if (fd >= 0) {
    if (ioctl(fd, I2C_SLAVE, addr) < 0) {
      auto e = errno;
      std::cerr << "i2c failed to set slave addr of "
        << std::hex << static_cast<int>(addr) << " " << syserror(e) << std::endl;
      end();
    }
  }
}

void TwoWire::endTransmission() {
  if (fd >= 0) {
    auto n = tx_next - tx_buf.begin();
    if (::write(fd, tx_buf.data(), n) != n) {
      auto e = errno;
      std::cerr << "i2c failed to write " << n << " bytes "
        << syserror(e) << std::endl;
      end();
    }
  }
  tx_next = tx_buf.begin();
}

void TwoWire::write(uint8_t c) {
  if (tx_next != tx_buf.end())
    *tx_next++ = c;
  else
    std::cerr << "i2c write buffer full" << std::endl;
}

TwoWire Wire;

