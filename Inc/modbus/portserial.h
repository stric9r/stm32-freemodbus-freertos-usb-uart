#ifndef PORTSERIAL_H
#define PORTSERIAL_H

// Serial port settings
#define DEFAULT_MODE        MB_RTU
#define DEFAULT_BAUDERATE   115200
#define DEFAULT_PARITY      MB_PAR_NONE
#define DEFAULT_STOP_BITS   1u

/* Select active transport — set for your product */
#define COMMS_MODBUS_UART     0
#define COMMS_MODBUS_USB      1
#define COMMS_MODBUS_DYNAMIC  2

#define COMMS_MODBUS_PORT     COMMS_MODBUS_DYNAMIC

#endif /* PORTSERIAL_H */
