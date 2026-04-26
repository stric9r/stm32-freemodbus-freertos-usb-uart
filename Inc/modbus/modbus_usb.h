#ifndef MODBUS_USB_H
#define MODBUS_USB_H

#include "mb.h"

void         modbus_usb_init(void);
eMBErrorCode modbus_usb_run(void);

#endif /* MODBUS_USB_H */
