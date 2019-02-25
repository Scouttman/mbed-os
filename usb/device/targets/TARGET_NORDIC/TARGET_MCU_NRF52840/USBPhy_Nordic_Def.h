/* mbed Microcontroller Library
 * Copyright (c) 2018-2018 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

extern "C" {
#include "nrf_usbd.h"
}

#ifndef USBPHY_NORDIC_DEF_H
#define USBPHY_NORDIC_DEF_H

#define MAX_PACKET_SIZE_SETUP 64
#define MAX_PACKET_NON_ISO    64
#define MAX_PACKET_ISO        1024
#define ISO_SPLIT_SIZE		  (MAX_PACKET_ISO >> 1)
#define ENDPOINT_NON_ISO      (USB_EP_ATTR_ALLOW_BULK | USB_EP_ATTR_ALLOW_INT)

#define EP_MAX_PACKET_SIZE(ep) (NRF_USBD_EPISO_CHECK(ep)? ISO_SPLIT_SIZE : MAX_PACKET_NON_ISO)


// If this bit is set in setup.bmRequestType, the setup transfer
// is DEVICE->HOST (IN transfer)
// if it is clear, the transfer is HOST->DEVICE (OUT transfer)
#define SETUP_TRANSFER_DIR_MASK 0x80

#define NUM_ENDPOINTS (NRF_USBD_EPIN_CNT + NRF_USBD_EPOUT_CNT)

#endif
