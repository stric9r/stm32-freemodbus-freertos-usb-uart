#ifndef MODBUS_PORT_OWNERSHIP_H
#define MODBUS_PORT_OWNERSHIP_H

#include <stdbool.h>

void modbus_port_ownership_init(void);
bool modbus_port_ownership_try_claim(void);
void modbus_port_ownership_refresh(void);

#endif /* MODBUS_PORT_OWNERSHIP_H */
