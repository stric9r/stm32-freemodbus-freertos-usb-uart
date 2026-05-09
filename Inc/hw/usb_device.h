/**
  ******************************************************************************
  * @file           : usb_device.h
  * @version        : v3.0_Cube
  * @brief          : Header for usb_device.c file.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
#ifndef __USB_DEVICE__H__
#define __USB_DEVICE__H__

#ifdef __cplusplus
 extern "C" {
#endif

#include "stm32l5xx.h"
#include "stm32l5xx_hal.h"
#include "usbd_def.h"


/** USB Device initialization function. */
void MX_USB_Device_Init(void);

/** Returns a pointer to the USB device handle owned by usb_device.c. */
USBD_HandleTypeDef * USB_GetDeviceHandle(void);

#ifdef __cplusplus
}
#endif

#endif /* __USB_DEVICE__H__ */

