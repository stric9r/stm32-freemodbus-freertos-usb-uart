/**
  ******************************************************************************
  * @file           : usb_device.c
  * @version        : v3.0_Cube
  * @brief          : This file implements the USB Device
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

#include "usb_device.h"

#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_cdc.h"
#include "usbd_cdc_if.h"

#include <assert.h>


/* USB Device Core handle declaration. */
USBD_HandleTypeDef hUsbDeviceFS;
extern USBD_DescriptorsTypeDef CDC_Desc;


/**
  * Init USB device Library, add supported class and start the library
  * @retval None
  */
void usb_device_init(void)
{
  /* Init Device Library, add supported class and start the library. */
  assert(USBD_OK == USBD_Init(&hUsbDeviceFS, &CDC_Desc, DEVICE_FS));
  assert(USBD_OK == USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC));
  assert(USBD_OK == USBD_CDC_RegisterInterface(&hUsbDeviceFS, &USBD_Interface_fops_FS));
  assert(USBD_OK == USBD_Start(&hUsbDeviceFS));
}

