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

// Name that appears on the BLE scanner
#define BLE_NAME "Nimble Example with RiotOS"

#define GATT_DEVICE_INFO_UUID                   0x180A
#define GATT_MANUFACTURER_NAME_UUID             0x2A29
#define GATT_MODEL_NUMBER_UUID                  0x2A24

#define STR_ANSWER_BUFFER_SIZE 100

#define HRS_FLAGS_DEFAULT   (0x01) /* 16-bit BPM value */
#define SENSOR_LOCATION     (0x02)   /* wrist sensor */
#define UPDATE_INTERVAL     (250U)
#define BPM_MIN             (80U)
#define BPM_MAX             (210U)
#define BPM_STEP            (2)
#define BAT_LEVEL           (42U)

/* ----------------------  Variables --------------------- */

// Names of the Device Info service Characteristics
static const char *_manufacturer_name = "Alvarado Inc.";
static const char *_model_number = "A4";
static const char *_serial_number = "15263748-9876-x4";
static const char *_fw_ver = "0.0.1";
static const char *_hw_ver = "1.6";

/* ----------------------  Prototypes --------------------- */

static int _devinfo_handler(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg);

static int _bas_handler(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt, void *arg);

static int _temp_handler(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt, void *arg);

int16_t read_temperature(void);

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
             .flags = BLE_GATT_CHR_F_READ,
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

/* ----------------------  Main  --------------------- */

int main(void)
{
    puts("NimBLE GATT Server Example");

    int rc = 0;
    (void)rc;

    /* verify and add our custom services */
    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    assert(rc == 0);
    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    assert(rc == 0);

    /* set the device name */
    ble_svc_gap_device_name_set(BLE_NAME);
    /* reload the GATT server to link our added services */
    ble_gatts_start();

    /* start to advertise this node */
    nimble_autoadv_start(NULL);

    return 0;
}
