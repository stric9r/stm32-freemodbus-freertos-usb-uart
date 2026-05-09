#ifndef MODBUS_USB_H
#define MODBUS_USB_H

#include "mb.h"

#ifdef __cplusplus
extern "C" {
#endif

void         modbus_usb_init(void);
eMBErrorCode modbus_usb_run(void);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_USB_H */
