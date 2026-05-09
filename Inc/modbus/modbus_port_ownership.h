/**
  ******************************************************************************
  * @file           : modbus_port_ownership.h
  * @brief          : Modbus bus ownership arbitration API
  ******************************************************************************
  * Stric Roberts, 2026. MIT License — see LICENSE in the project root.
  ******************************************************************************
  */
#ifndef MODBUS_PORT_OWNERSHIP_H
#define MODBUS_PORT_OWNERSHIP_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void modbus_port_ownership_init(void);
bool modbus_port_ownership_try_claim(void);
void modbus_port_ownership_refresh(void);
void modbus_port_ownership_release(void);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_PORT_OWNERSHIP_H */
