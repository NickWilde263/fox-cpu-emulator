#ifndef _PORT_IO_H
#define _PORT_IO_H

extern "C" {
  void writePort(uint8_t portNumber, uint8_t data);
  uint8_t readPort(uint8_t portNumber);
}

#endif
