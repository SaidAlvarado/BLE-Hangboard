/*
 * Copyright (C) 2018 Freie Universität Berlin
 *               2018 Codecoup
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     examples
 * @{
 *
 * @file
 * @brief       BLE peripheral example using NimBLE
 *
 * Have a more detailed look at the api here:
 * https://mynewt.apache.org/latest/tutorials/ble/bleprph/bleprph.html
 *
 * More examples (not ready to run on RIOT) can be found here:
 * https://github.com/apache/mynewt-nimble/tree/master/apps
 *
 * Test this application e.g. with Nordics "nRF Connect"-App
 * iOS: https://itunes.apple.com/us/app/nrf-connect/id1054362403
 * Android: https://play.google.com/store/apps/details?id=no.nordicsemi.android.mcp
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 * @author      Andrzej Kaczmarek <andrzej.kaczmarek@codecoup.pl>
 * @author      Hendrik van Essen <hendrik.ve@fu-berlin.de>
 *
 * @}
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <nrf.h>

#include "event/timeout.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "net/bluetil/ad.h"
#include "nimble_autoadv.h"
#include "nimble_riot.h"

#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

/* ----------------------  Defines --------------------- */
#define BLE_GATT_SVC_ESS 0x181A         // Environmental Sensing Service
#define BLE_GATT_CHAR_TEMP 0x2A6E       // Temperature Characteristic

#define GATT_DEVICE_INFO_UUID                   0x180A
#define GATT_MANUFACTURER_NAME_UUID             0x2A29
#define GATT_MODEL_NUMBER_UUID                  0x2A24

#define UPDATE_INTERVAL     (250U)   // miliseconds between temperature updates
#define BAT_LEVEL           (42U)

/* ----------------------  Variables --------------------- */

// Names of the Device Info service Characteristics
static const char *_manufacturer_name = "Alvarado Inc.";
static const char *_model_number = "A4";
static const char *_serial_number = "15263748-9876-x4";
static const char *_fw_ver = "0.0.1";
static const char *_hw_ver = "1.6";

// Global variable for the temperature notify
static uint16_t _temp_val_handle;  // THis is not the temperature value, is just a kind of like an UUID for identifying the actual value.
                                    // It HAS to be a uint16_t, irrespective of what the actual data is.
static uint16_t _conn_handle;     // Handle for the BLE connection

// Periodic event callback  variables
static event_queue_t _eq;
static event_t _update_evt;
static event_timeout_t _update_timeout_evt;

/* ----------------------  Prototypes --------------------- */

static int
_devinfo_handler(uint16_t conn_handle, uint16_t attr_handle,
                    struct ble_gatt_access_ctxt *ctxt, void *arg);

static int _bas_handler(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt, void *arg);

static int _temp_handler(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt, void *arg);

static void _temp_update(event_t *e);

int16_t read_temperature(void);

void init_rng(void);
uint8_t read_rng(void);

/* ----------------------  GATT SERVICE DEFINITION --------------------- */

/* define several bluetooth services for our device */
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    /*
     * access_cb defines a callback for read and write access events on
     * given characteristics
     */
    {
        /* Device Information Service */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_GATT_SVC_DEVINFO),
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = BLE_UUID16_DECLARE(BLE_GATT_CHAR_MANUFACTURER_NAME),
                .access_cb = _devinfo_handler,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {
                .uuid = BLE_UUID16_DECLARE(BLE_GATT_CHAR_MODEL_NUMBER_STR),
                .access_cb = _devinfo_handler,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {
                .uuid = BLE_UUID16_DECLARE(BLE_GATT_CHAR_SERIAL_NUMBER_STR),
                .access_cb = _devinfo_handler,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {
                .uuid = BLE_UUID16_DECLARE(BLE_GATT_CHAR_FW_REV_STR),
                .access_cb = _devinfo_handler,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {
                .uuid = BLE_UUID16_DECLARE(BLE_GATT_CHAR_HW_REV_STR),
                .access_cb = _devinfo_handler,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {
                0, /* no more characteristics in this service */
            },
        }},
    {/* Battery Level Service */
     .type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = BLE_UUID16_DECLARE(BLE_GATT_SVC_BAS),
     .characteristics = (struct ble_gatt_chr_def[]){
         {
             .uuid = BLE_UUID16_DECLARE(BLE_GATT_CHAR_BATTERY_LEVEL),
             .access_cb = _bas_handler,
             .flags = BLE_GATT_CHR_F_READ,
         },
         {
             0, /* no more characteristics in this service */
         },
     }},
    {/* Environmental Sensing Service */
     .type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = BLE_UUID16_DECLARE(BLE_GATT_SVC_ESS),
     .characteristics = (struct ble_gatt_chr_def[]){
         {
             .uuid = BLE_UUID16_DECLARE(BLE_GATT_CHAR_TEMP),
             .access_cb = _temp_handler,
             .val_handle = &_temp_val_handle,
             .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
         },
         {
             0, /* no more characteristics in this service */
         },
     }},
    {
        0, /* No more services */
    },
};

/* ----------------------  Public  --------------------- */

static int _devinfo_handler(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg) {
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;
    const char *str;

    switch (ble_uuid_u16(ctxt->chr->uuid)) {
    case BLE_GATT_CHAR_MANUFACTURER_NAME:
        puts("[READ] device information service: manufacturer name value");
        str = _manufacturer_name;
        break;
    case BLE_GATT_CHAR_MODEL_NUMBER_STR:
        puts("[READ] device information service: model number value");
        str = _model_number;
        break;
    case BLE_GATT_CHAR_SERIAL_NUMBER_STR:
        puts("[READ] device information service: serial number value");
        str = _serial_number;
        break;
    case BLE_GATT_CHAR_FW_REV_STR:
        puts("[READ] device information service: firmware revision value");
        str = _fw_ver;
        break;
    case BLE_GATT_CHAR_HW_REV_STR:
        puts("[READ] device information service: hardware revision value");
        str = _hw_ver;
        break;
    default:
        return BLE_ATT_ERR_UNLIKELY;
    }

    int res = os_mbuf_append(ctxt->om, str, strlen(str));
    return (res == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int _bas_handler(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt, void *arg) {
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    puts("[READ] battery level service: battery level value");

    uint8_t level = BAT_LEVEL; /* this battery will never drain :-) */
    int res = os_mbuf_append(ctxt->om, &level, sizeof(level));
    return (res == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int _temp_handler(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt, void *arg) {
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    puts("[READ] Environmental Sensing service: Temperature value");

    /* Get the current temperature value from the sensor */
    int16_t temperature = read_temperature();
    /* Convert the temperature to a 16-bit signed integer in units of 0.01 degrees Celsius */
    temperature = temperature * 100;

    int res = os_mbuf_append(ctxt->om, &temperature, sizeof(temperature));
    return (res == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

int16_t read_temperature(void) {
    // Enable the internal temperature sensor
    NRF_TEMP->TASKS_START = 1;

    // Wait for the temperature measurement to complete
    while (NRF_TEMP->EVENTS_DATARDY == 0)
        ;

    // Read the temperature measurement from the data register
    int16_t temperature = (int16_t)NRF_TEMP->TEMP / 4; // Data is registered in the register in 0.25 degC steps

    // Clear the data ready event
    NRF_TEMP->EVENTS_DATARDY = 0;

    // Disable the internal temperature sensor
    NRF_TEMP->TASKS_STOP = 1;

    // Return the temperature measurement
    return temperature;
}

void init_rng(void) {

    // Enable the RNG peripheral
    NRF_RNG->TASKS_START = 1;

    // Enable the bias correction
    NRF_RNG->CONFIG |= RNG_CONFIG_DERCEN_Msk;

    // Wait for the bias correction to take effect
    while ((NRF_RNG->EVENTS_VALRDY == 0)) {;}

    // Clear the value ready and error events
    NRF_RNG->EVENTS_VALRDY = 0;
}

uint8_t read_rng(void){

    // Wait for a new random number to be generated
    while (NRF_RNG->EVENTS_VALRDY == 0);

    // Read a byte from the RNG
    uint8_t random_byte = NRF_RNG->VALUE;

    // Clear the value ready event
    NRF_RNG->EVENTS_VALRDY = 0;

    // Return the random byte
    return random_byte;
}

static void _temp_update(event_t *e) {
    (void)e;
    struct os_mbuf *om;


    /* Calculate the new random Temperature value */
    int16_t temperature = (int16_t)read_rng() * 10; // values from 0 to 25.5 degC

    printf("[NOTIFY] Temperature Characteistic: measurement %i\n", (int)temperature);

    /* send heart rate data notification to GATT client */
    om = ble_hs_mbuf_from_flat(&temperature, sizeof(temperature));
    assert(om != NULL);
    int res = ble_gatts_notify_custom(_conn_handle, _temp_val_handle, om);
    assert(res == 0);
    (void)res;

    /* schedule next update event */
    event_timeout_set(&_update_timeout_evt, UPDATE_INTERVAL);
}

static void _start_updating(void) {
    event_timeout_set(&_update_timeout_evt, UPDATE_INTERVAL);
    puts("[NOTIFY_ENABLED] Temperature sensing service");
}

static void _stop_updating(void) {
    event_timeout_clear(&_update_timeout_evt);
    puts("[NOTIFY_DISABLED] Temperature sensing service");
}

static int gap_event_cb(struct ble_gap_event *event, void *arg) {
    (void)arg;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status) {
            _stop_updating();
            nimble_autoadv_start(NULL);
            return 0;
        }
        _conn_handle = event->connect.conn_handle;
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        _stop_updating();
        nimble_autoadv_start(NULL);
        break;

    case BLE_GAP_EVENT_SUBSCRIBE:
        if (event->subscribe.attr_handle == _temp_val_handle) {
            if (event->subscribe.cur_notify == 1) {
                _start_updating();
            } else {
                _stop_updating();
            }
        }
        break;
    }

    return 0;
}

/* ----------------------  Main  --------------------- */

int main(void)
{
    puts("NimBLE GATT Server Example");

    // Initialize random number generator
    init_rng();

    int rc = 0;
    (void)rc;

    // Create the event Callbacks to periodically update the Temperature update.
    event_queue_init(&_eq);
    _update_evt.handler = _temp_update;
    event_timeout_ztimer_init(&_update_timeout_evt, ZTIMER_MSEC, &_eq, &_update_evt);

    /* verify and add our custom services */
    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    assert(rc == 0);
    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    assert(rc == 0);

    /* set the device name */
    ble_svc_gap_device_name_set(CONFIG_NIMBLE_AUTOADV_DEVICE_NAME);
    /* reload the GATT server to link our added services */
    ble_gatts_start();

    // Configure the notification and ble connectiong advertissment
    nimble_autoadv_cfg_t cfg = {
        .adv_duration_ms = BLE_HS_FOREVER,
        .adv_itvl_ms = BLE_GAP_ADV_ITVL_MS(100),
        .flags = NIMBLE_AUTOADV_FLAG_CONNECTABLE | NIMBLE_AUTOADV_FLAG_LEGACY |
                 NIMBLE_AUTOADV_FLAG_SCANNABLE,
        .channel_map = 0,
        .filter_policy = 0,
        .own_addr_type = nimble_riot_own_addr_type,
        .phy = NIMBLE_PHY_1M,
        .tx_power = 0,
    };
    /* set advertise params */
    nimble_autoadv_cfg_update(&cfg);
    /* configure and set the advertising data */
    uint16_t temp_uuid = BLE_GATT_SVC_ESS;
    nimble_autoadv_add_field(BLE_GAP_AD_UUID16_INCOMP, &temp_uuid, sizeof(temp_uuid));
    nimble_autoadv_set_gap_cb(&gap_event_cb, NULL);

    /* start to advertise this node */
    nimble_autoadv_start(NULL);

    /* run an event loop for handling the temperature rate update events */
    event_loop(&_eq);

    return 0;
}
