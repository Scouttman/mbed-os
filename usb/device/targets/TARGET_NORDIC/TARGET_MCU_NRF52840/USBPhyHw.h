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

#ifndef USBPHYHW_H
#define USBPHYHW_H

#include "USBPhy.h"
#include "USBPhy_Nordic_Def.h"

extern "C" {
#include "nrf_drv_power.h"
}

class USBPhyHw : public USBPhy {
public:
    USBPhyHw();
    virtual ~USBPhyHw();
    virtual void init(USBPhyEvents *events);
    virtual void deinit();
    virtual bool powered();
    virtual void connect();
    virtual void disconnect();
    virtual void configure();
    virtual void unconfigure();
    virtual void sof_enable();
    virtual void sof_disable();
    virtual void set_address(uint8_t address);
    virtual void remote_wakeup();
    virtual const usb_ep_table_t* endpoint_table();

    virtual uint32_t ep0_set_max_packet(uint32_t max_packet);
    virtual void ep0_setup_read_result(uint8_t *buffer, uint32_t size);
    virtual void ep0_read(uint8_t *data, uint32_t size);
    virtual uint32_t ep0_read_result();
    virtual void ep0_write(uint8_t *buffer, uint32_t size);
    virtual void ep0_stall();

    virtual bool endpoint_add(usb_ep_t endpoint, uint32_t max_packet, usb_ep_type_t type);
    virtual void endpoint_remove(usb_ep_t endpoint);
    virtual void endpoint_stall(usb_ep_t endpoint);
    virtual void endpoint_unstall(usb_ep_t endpoint);

    virtual bool endpoint_read(usb_ep_t endpoint, uint8_t *data, uint32_t size);
    virtual uint32_t endpoint_read_result(usb_ep_t endpoint);
    virtual bool endpoint_write(usb_ep_t endpoint, uint8_t *data, uint32_t size);
    virtual void endpoint_abort(usb_ep_t endpoint);

    virtual void process();

private:

    // Structure to hold information about an endpoint
    typedef struct usb_ep_info_t {
    		uint32_t max_packet_size;
    } usb_ep_info_t;

    USBPhyEvents *events;

    bool started;

    bool connect_enabled;

    // DMA flags
    volatile bool dma_busy;
	volatile uint16_t ep_waiting_for_dma;

	// Endpoint information structures
	usb_ep_info_t ep_info[NUM_ENDPOINTS];

private:

	inline static uint8_t get_ep_index(usb_ep_t ep) {
		return ((NRF_USBD_EPIN_CHECK(ep)? 8 : 0) + NRF_USBD_EP_NR_GET(ep));
	}

	/*
	 * Disables all endpoints except for control (IN0/OUT0)
	 */
	void default_config(void);

	void _usbd_enable(void);
	void _usbd_disable(void);

	/*
	 * Called after USBPWRRDY event
	 * This means the USB PHY regulator has
	 * stabilized and is ready to go
	 */
	void _usbd_start(void);
	void _usbd_stop(void);


    static void _usbisr(void);
    static void _usb_power_evt_handler(nrf_drv_power_usb_evt_t event);

    static void _enable_usb_interrupts(void);
    static void _disable_usb_interrupts(void);
};

#endif
