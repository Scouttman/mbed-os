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

#include "USBPhyHw.h"

static USBPhyHw *instance = NULL;

// Power related flags
static bool usb_power_evt_flag;
static nrf_drv_power_usb_evt_t usb_power_evt;

USBPhy *get_usb_phy()
{
    static USBPhyHw usbphy;
    return &usbphy;
}

USBPhyHw::USBPhyHw(): events(NULL), started(false), connect_enabled(false)
{
	usb_power_evt_flag = false;
	usb_power_evt = NRF_DRV_POWER_USB_EVT_REMOVED;
}

USBPhyHw::~USBPhyHw()
{

}

void USBPhyHw::init(USBPhyEvents *events)
{
    this->events = events;

    // Disable IRQ
    _disable_usb_interrupts();
    NVIC_DisableIRQ(POWER_CLOCK_IRQn);

    // Store reference to this instance
	instance = this;

    // Initialize the power driver
    ret_code_t ret = nrf_drv_power_init(NULL);

    // Register a handler for USB power events
    static const nrf_drv_power_usbevt_config_t config = { .handler =
    		_usb_power_evt_handler };

    // This will enable USB-related power interrupts
    ret = nrf_drv_power_usbevt_init(&config);

    // Configure endpoint info structures
    int n;
    for(n = 0; n < NRF_USBD_EPOUT_CNT; n++) {
    		usb_ep_info_t* info = &this->ep_info[get_ep_index(NRF_USBD_EPOUT(n))];
    		info->max_packet_size = EP_MAX_PACKET_SIZE(n);
    }

    for(n = 0; n < NRF_USBD_EPIN_CNT; n++) {
    		usb_ep_info_t* info = &this->ep_info[get_ep_index(NRF_USBD_EPIN(n))];
    		info->max_packet_size = EP_MAX_PACKET_SIZE(n);
    }

    // Enable IRQ (not USB yet, see _usbd_start)
    NVIC_SetVector(USBD_IRQn, (uint32_t)&_usbisr);
    NVIC_EnableIRQ(POWER_CLOCK_IRQn);
}

void USBPhyHw::deinit()
{
    // Disconnect and disable interrupt
    disconnect();
    NVIC_DisableIRQ(USBD_IRQn);

    instance = NULL;
}

bool USBPhyHw::powered()
{
	nrf_drv_power_usb_state_t state = nrf_drv_power_usbstatus_get();
	if (state == NRF_DRV_POWER_USB_STATE_CONNECTED
	 || state == NRF_DRV_POWER_USB_STATE_READY)
		return true;
	else
		return false;
}

void USBPhyHw::connect()
{
	// To save power, we only enable the USBD peripheral
	// when there's actually VBUS detected

	// So flag that the USB stack is ready to connect
	this->connect_enabled = true;

	// If VBUS is already available, enable immediately
	if(nrf_drv_power_usbstatus_get() == NRF_DRV_POWER_USB_STATE_CONNECTED)
	{
		// Enabling USB will cause NRF_DRV_POWER_USB_EVT_READY
		// to occur, which will start the USBD peripheral
		// when the internal regulator has settled
		_usbd_enable();

		// Check to see if the regulator is ready now
		if(nrf_drv_power_usbstatus_get() == NRF_DRV_POWER_USB_STATE_READY)
			if(!this->started)
				_usbd_start();
	}
}

void USBPhyHw::disconnect()
{
	this->connect_enabled = false;

	// TODO - see what nordic driver does when disabling all endpoints
	// TODO - datasheet recommends to let EasyDMA transfers finish before disabling USBD
	nrf_usbd_ep_all_disable();
	nrf_usbd_pullup_disable();
	nrf_usbd_disable();

    // TODO - Disable all endpoints

    // TODO - Clear all endpoint interrupts

    // TODO - Disable pullup on D+
}

void USBPhyHw::configure()
{
    // TODO - set device to configured. Most device will not need this
}

void USBPhyHw::unconfigure()
{
	// Remove all endpoints (except control)
	default_config();
}

void USBPhyHw::sof_enable()
{
    nrf_usbd_int_enable(NRF_USBD_INT_SOF_MASK);
}

void USBPhyHw::sof_disable()
{
	nrf_usbd_int_disable(NRF_USBD_INT_SOF_MASK);
}

void USBPhyHw::set_address(uint8_t address)
{
	// Handled by hardware -- do nothing
	(void) address;
}

void USBPhyHw::remote_wakeup()
{
    // TODO - Sent remote wakeup over USB lines (if supported)
}

const usb_ep_table_t *USBPhyHw::endpoint_table()
{
	static const usb_ep_table_t ep_table =
		{
			2048, // 64 bytes per bulk/int endpoint (16), 1023 bytes for iso endpoint pair (1)
			{
				{ USB_EP_ATTR_ALLOW_CTRL		| USB_EP_ATTR_DIR_IN_AND_OUT, 0, 0 },
				{ ENDPOINT_NON_ISO 			| USB_EP_ATTR_DIR_IN_AND_OUT, 0, 0 },
				{ ENDPOINT_NON_ISO 			| USB_EP_ATTR_DIR_IN_AND_OUT, 0, 0 },
				{ ENDPOINT_NON_ISO 			| USB_EP_ATTR_DIR_IN_AND_OUT, 0, 0 },
				{ ENDPOINT_NON_ISO			| USB_EP_ATTR_DIR_IN_AND_OUT, 0, 0 },
				{ ENDPOINT_NON_ISO 			| USB_EP_ATTR_DIR_IN_AND_OUT, 0, 0 },
				{ ENDPOINT_NON_ISO 			| USB_EP_ATTR_DIR_IN_AND_OUT, 0, 0 },
				{ ENDPOINT_NON_ISO 			| USB_EP_ATTR_DIR_IN_AND_OUT, 0, 0 },
				{ USB_EP_ATTR_ALLOW_ISO 		| USB_EP_ATTR_DIR_IN_AND_OUT, 0, 0 },
				{ 0 							| USB_EP_ATTR_DIR_IN_AND_OUT, 0, 0 },
				{ 0							| USB_EP_ATTR_DIR_IN_AND_OUT, 0, 0 },
				{ 0							| USB_EP_ATTR_DIR_IN_AND_OUT, 0, 0 },
				{ 0 							| USB_EP_ATTR_DIR_IN_AND_OUT, 0, 0 },
				{ 0							| USB_EP_ATTR_DIR_IN_AND_OUT, 0, 0 },
				{ 0							| USB_EP_ATTR_DIR_IN_AND_OUT, 0, 0 },
				{ 0							| USB_EP_ATTR_DIR_IN_AND_OUT, 0, 0 },
			}
		};
	return &ep_table;
}

uint32_t USBPhyHw::ep0_set_max_packet(uint32_t max_packet)
{
    // TODO - set endpoint 0 size and return this size
    return 64;
}

// read setup packet
void USBPhyHw::ep0_setup_read_result(uint8_t *buffer, uint32_t size)
{
    // TODO - read up to size bytes of the setup packet
}

void USBPhyHw::ep0_read(uint8_t *data, uint32_t size)
{
    // TODO - setup data buffer to receive next endpoint 0 OUT packet
}

uint32_t USBPhyHw::ep0_read_result()
{
    // TODO - return the size of the last OUT packet received on endpoint 0
    return 0;
}

void USBPhyHw::ep0_write(uint8_t *buffer, uint32_t size)
{
    // TODO - start transferring buffer on endpoint 0 IN
}

void USBPhyHw::ep0_stall()
{
    // TODO - protocol stall endpoint 0. This stall must be automatically
    //        cleared by the next setup packet
}

bool USBPhyHw::endpoint_add(usb_ep_t endpoint, uint32_t max_packet, usb_ep_type_t type)
{
    // TODO - enable this endpoint

    return true;
}

void USBPhyHw::endpoint_remove(usb_ep_t endpoint)
{
    // TODO - disable and remove this endpoint
}

void USBPhyHw::endpoint_stall(usb_ep_t endpoint)
{
    // TODO - stall this endpoint until it is explicitly cleared
}

void USBPhyHw::endpoint_unstall(usb_ep_t endpoint)
{
    // TODO - unstall this endpoint
}

bool USBPhyHw::endpoint_read(usb_ep_t endpoint, uint8_t *data, uint32_t size)
{
    // TODO - setup data buffer to receive next endpoint OUT packet and return true if successful
    return true;
}

uint32_t USBPhyHw::endpoint_read_result(usb_ep_t endpoint)
{
    // TODO - return the size of the last OUT packet received on endpoint
}

bool USBPhyHw::endpoint_write(usb_ep_t endpoint, uint8_t *data, uint32_t size)
{
    // TODO - start transferring buffer on endpoint IN

    return true;
}

void USBPhyHw::endpoint_abort(usb_ep_t endpoint)
{
    // TODO - stop the current transfer on this endpoint and don't call the IN or OUT callback
}

void USBPhyHw::process()
{
    // TODO - update register for your mcu

    //uint8_t stat = USB0->STAT;

    //USB0->STAT = stat; // Clear pending interrupts

    // reset interrupt
    /*if (stat & USB_STAT_RESET_MASK) {

        // TODO - disable all endpoints

        // TODO - clear all endpoint interrupts

        // TODO - enable control endpoint

        // reset bus for USBDevice layer
        events->reset();

        // Re-enable interrupt
        NVIC_ClearPendingIRQ(USB_IRQn);
        NVIC_EnableIRQ(USB_IRQn);
        return;
    }*/

    // A usb power event happened
    if(usb_power_evt_flag) {

    		// Power applied
    		if(usb_power_evt == NRF_DRV_POWER_USB_EVT_DETECTED) {
    			events->power(true);

    			// Start connecting if connect() was already called
    			if(connect_enabled)
    				_usbd_enable();
    		}

    		// Power lost
    		if(usb_power_evt == NRF_DRV_POWER_USB_EVT_REMOVED) {
    			events->power(false);
    		}

    		if(usb_power_evt == NRF_DRV_POWER_USB_EVT_READY) {
    			// USB power is ready, start USBD processing
    			if(!this->started)
    				_usbd_start();
    		}

    		usb_power_evt_flag = false;
    }

    // sleep interrupt
//    if (stat & USB_STAT_SUSPEND_MASK) {
//        events->suspend(true);
//    }
//
//    // resume interrupt
//    if (stat & USB_STAT_RESUME_MASK) {
//        events->suspend(false);
//    }
//
//    // sof interrupt
//    if (stat & USB_STAT_SOF_MASK) {
//        // SOF event, read frame number
//        events->sof(USB0->FRAME);
//    }
//
//    // endpoint interrupt
//    if (stat & USB_STAT_EP_MASK) {
//        uint32_t ep_pending = USB->EP;
//
//        // TODO - call endpoint 0 IN callback if pending
//        events->ep0_in();
//
//        // TODO - call endpoint 0 OUT callback if pending
//        events->ep0_out();
//
//        // TODO - call endpoint 0 SETUP callback if pending
//        events->ep0_setup();
//
//        for (int i = 0; i < 16; i++) {
//            // TODO - call endpoint i IN callback if pending
//            events->in(0x80 | i);
//        }
//
//        for (int i = 0; i < 16; i++) {
//            // TODO - call endpoint i OUT callback if pending
//            events->out();
//        }
//
//        USB->EP = ep_pending; // clear pending
//    }

    // Re-enable interrupt
    _enable_usb_interrupts();
}

/*
 * Enables the USBD peripheral (with all the crazy errata fixes)
 * @note blocks until USBEVENT READY is received (a few us)
 */
void USBPhyHw::_usbd_enable(void) {
	/* Prepare for READY event receiving */
	nrf_usbd_eventcause_clear(NRF_USBD_EVENTCAUSE_READY_MASK);

	// Errata 187
	if (*((volatile uint32_t *)(0x4006EC00)) == 0x00000000)
	{
		*((volatile uint32_t *)(0x4006EC00)) = 0x00009375;
		*((volatile uint32_t *)(0x4006ED14)) = 0x00000003;
		*((volatile uint32_t *)(0x4006EC00)) = 0x00009375;
	}
	else
	{
		*((volatile uint32_t *)(0x4006ED14)) = 0x00000003;
	}

	// Errata 171
	if (*((volatile uint32_t *)(0x4006EC00)) == 0x00000000)
	{
		*((volatile uint32_t *)(0x4006EC00)) = 0x00009375;
		*((volatile uint32_t *)(0x4006ED14)) = 0x00000003;
		*((volatile uint32_t *)(0x4006EC00)) = 0x00009375;
	}
	else
	{
		*((volatile uint32_t *)(0x4006ED14)) = 0x00000003;
	}

	/* Enable the peripheral */
	nrf_usbd_enable();
	/* Waiting for peripheral to enable, this should take a few us */
	while (0 == (NRF_USBD_EVENTCAUSE_READY_MASK & nrf_usbd_eventcause_get()))
	{
		/* Empty loop */
	}
	nrf_usbd_eventcause_clear(NRF_USBD_EVENTCAUSE_READY_MASK);

	// Errata 171
   if (*((volatile uint32_t *)(0x4006EC00)) == 0x00000000)
   {
	   *((volatile uint32_t *)(0x4006EC00)) = 0x00009375;
	   *((volatile uint32_t *)(0x4006EC14)) = 0x00000000;
	   *((volatile uint32_t *)(0x4006EC00)) = 0x00009375;
   }
   else
   {
	   *((volatile uint32_t *)(0x4006EC14)) = 0x00000000;
   }

   // Errata 166
	*((volatile uint32_t *)(NRF_USBD_BASE + 0x800)) = 0x7E3;
	*((volatile uint32_t *)(NRF_USBD_BASE + 0x804)) = 0x40;
	__ISB();
	__DSB();

	nrf_usbd_isosplit_set(NRF_USBD_ISOSPLIT_Half);

	// TODO - implement these kind of flags
	//m_ep_ready = (((1U << NRF_USBD_EPIN_CNT) - 1U) << USBD_EPIN_BITPOS_0);
	//m_ep_dma_waiting = 0;
	//m_dma_pending    = 0;
	//m_last_setup_dir = NRF_DRV_USBD_EPOUT0;
}

void USBPhyHw::default_config(void) {
	nrf_usbd_int_disable(
		NRF_USBD_INT_ENDEPIN1_MASK  |
		NRF_USBD_INT_ENDEPIN2_MASK  |
		NRF_USBD_INT_ENDEPIN3_MASK  |
		NRF_USBD_INT_ENDEPIN4_MASK  |
		NRF_USBD_INT_ENDEPIN5_MASK  |
		NRF_USBD_INT_ENDEPIN6_MASK  |
		NRF_USBD_INT_ENDEPIN7_MASK  |
		NRF_USBD_INT_ENDISOIN0_MASK |
		NRF_USBD_INT_ENDEPOUT1_MASK |
		NRF_USBD_INT_ENDEPOUT2_MASK |
		NRF_USBD_INT_ENDEPOUT3_MASK |
		NRF_USBD_INT_ENDEPOUT4_MASK |
		NRF_USBD_INT_ENDEPOUT5_MASK |
		NRF_USBD_INT_ENDEPOUT6_MASK |
		NRF_USBD_INT_ENDEPOUT7_MASK |
		NRF_USBD_INT_ENDISOOUT0_MASK
	);
	nrf_usbd_int_enable(NRF_USBD_INT_ENDEPIN0_MASK | NRF_USBD_INT_ENDEPOUT0_MASK);
	nrf_usbd_ep_all_disable();
}

void USBPhyHw::_usbd_start(void) {

	this->started = true;

	uint32_t ints_to_enable =
	   NRF_USBD_INT_USBRESET_MASK     |
	   NRF_USBD_INT_STARTED_MASK      |
	   NRF_USBD_INT_ENDEPIN0_MASK     |
	   NRF_USBD_INT_EP0DATADONE_MASK  |
	   NRF_USBD_INT_ENDEPOUT0_MASK    |
	   NRF_USBD_INT_USBEVENT_MASK     |
	   NRF_USBD_INT_EP0SETUP_MASK     |
	   NRF_USBD_INT_DATAEP_MASK;

	/* Enable all required interrupts */
	nrf_usbd_int_enable(ints_to_enable);

	// Enable global USBD interrupts
	NVIC_SetPriority(USBD_IRQn, 7);
	NVIC_EnableIRQ(USBD_IRQn);

	// Enable the pull-ups
	nrf_usbd_pullup_enable();
}

void USBPhyHw::_usbd_stop(void) {
	this->started = false;
}

void USBPhyHw::_usbisr(void) {
	_disable_usb_interrupts();
	if(instance != NULL)
		instance->events->start_process();
}

void USBPhyHw::_usb_power_evt_handler(nrf_drv_power_usb_evt_t event) {
	_disable_usb_interrupts();
	usb_power_evt = event;
	usb_power_evt_flag = true;
	if(instance != NULL)
		instance->events->start_process();
}

void USBPhyHw::_enable_usb_interrupts(void) {
	// Enable USB and USB-related power interrupts
	NVIC_EnableIRQ(USBD_IRQn);
	nrf_power_int_enable(NRF_POWER_INT_USBDETECTED_MASK |
						 NRF_POWER_INT_USBREMOVED_MASK |
						 NRF_POWER_INT_USBPWRRDY_MASK);
}

void USBPhyHw::_disable_usb_interrupts(void) {
	// Disable USB and USB-related power interrupts
	NVIC_DisableIRQ(USBD_IRQn);
	nrf_power_int_disable(NRF_POWER_INT_USBDETECTED_MASK |
						 NRF_POWER_INT_USBREMOVED_MASK |
						 NRF_POWER_INT_USBPWRRDY_MASK);
}
