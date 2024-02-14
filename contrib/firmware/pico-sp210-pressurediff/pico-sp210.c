/*
 * Copyright (C) 2023-2024 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <inttypes.h>

#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/irq.h"
#include "hardware/i2c.h"


#define LED_PIN PICO_DEFAULT_LED_PIN
#define SP210_ADDR _u(0x28)
#define SP210_I2C_SCL_PIN 5
#define SP210_I2C_SDA_PIN 4
#define SP210_I2C_FREQ 115200
#define SP210_I2C_HWBLOCK i2c0


/* bitfield helpers */
#define BIT_SET(a,b) ((a) |= (1ULL<<(b)))
#define BIT_CLEAR(a,b) ((a) &= ~(1ULL<<(b)))
#define BIT_FLIP(a,b) ((a) ^= (1ULL<<(b)))
#define BIT_CHECK(a,b) (!!((a) & (1ULL<<(b))))

/* the sensor mode command bit */
uint8_t g_sensor_mode = 0b00011100;

/* update rate in Hz */
int g_update_rate_hz = 250;

/* set to true to initialize the sensor */
volatile bool g_init_sensor = true;

/* true if we are acquiring data */
volatile bool g_running = false;

/* true if we are supposed to read sensor info */
volatile bool g_send_info_block = false;

/* timebase for timestamps */
uint64_t g_base_timestamp_ms = 0;


static void
reset_run_timestamp()
{
    uint64_t time_us = time_us_64();
    g_base_timestamp_ms = time_us / 1000;
}

static uint64_t
current_run_timestamp_ms()
{
    uint64_t time_us = time_us_64();
    return (time_us / 1000) - g_base_timestamp_ms;
}

static int
sp210_read_datarate_from_config()
{
    bool b2, b3, b4;
    b2 = BIT_CHECK(g_sensor_mode, 2);
    b3 = BIT_CHECK(g_sensor_mode, 3);
    b4 = BIT_CHECK(g_sensor_mode, 4);

    if (b2 && !b3 && !b4)
        return 35;
    if (!b2 && b3 && !b4)
        return 50;
    if (b2 && b3 && !b4)
        return 65;
    if (!b2 && !b3 && b4)
        return 100;
    if (b2 && !b3 && b4)
        return 130;
    if (!b2 && b3 && b4)
        return 180;
    if (b2 && b3 && b4)
        return 250;

    return 25;
}

static void
check_process_command()
{
    int test_char;
    char buf[256];

    /* check if we have new input, immediately return if we haven't */
    test_char = getchar_timeout_us(0);
    if (test_char == PICO_ERROR_TIMEOUT)
        return;
    memset(buf, 0, sizeof(buf));
    buf[0] = test_char;

    /* wait to read a line from the input buffer */
    if (fgets(&buf[1], sizeof(buf) - 1, stdin) == NULL)
        return;

    printf("C:%s\n", (const char*)buf);

    if (strncmp(buf, "INFO", 4) == 0) {
        if (!g_running)
            g_send_info_block = true;
        return;
    }

    if (strncmp(buf, "START", 5) == 0) {
        reset_run_timestamp();
        gpio_put(LED_PIN, 1);
        g_init_sensor = true;
        g_running = true;
        return;
    }

    if (strncmp(buf, "STOP", 4) == 0) {
        g_running = false;
        gpio_put(LED_PIN, 0);
        return;
    }

    if (strncmp(buf, "SET_ZERO", 8) == 0) {
        /* we set the zero-point on initialization, so re-init is also zeroing */
        g_init_sensor = true;
        return;
    }

    if (strncmp(buf, "REINIT", 8) == 0) {
        g_init_sensor = true;
        return;
    }

    if (strncmp(buf, "ZERO_NOISE_SUPPRESSION", 22) == 0) {
        if (strncmp(&buf[22], "=true", 5) == 0)
            BIT_SET(g_sensor_mode, 7);
        else if (strncmp(&buf[22], "=false", 6) == 0)
            BIT_CLEAR(g_sensor_mode, 7);
        return;
    }

    if (strncmp(buf, "ZERO_MODE", 9) == 0) {
        if (strncmp(&buf[9], "=standard", 9) == 0)
            BIT_CLEAR(g_sensor_mode, 6);
        else if (strncmp(&buf[9], "=ztrack", 7) == 0)
            BIT_SET(g_sensor_mode, 6);
        return;
    }

    if (strncmp(buf, "RATE", 4) == 0) {
        if (strncmp(&buf[4], "=250", 4) == 0) {
            BIT_SET(g_sensor_mode, 2);
            BIT_SET(g_sensor_mode, 3);
            BIT_SET(g_sensor_mode, 4);
        } else if (strncmp(&buf[4], "=180", 4) == 0) {
            BIT_CLEAR(g_sensor_mode, 2);
            BIT_SET(g_sensor_mode, 3);
            BIT_SET(g_sensor_mode, 4);
        } else if (strncmp(&buf[4], "=130", 4) == 0) {
            BIT_SET(g_sensor_mode, 2);
            BIT_CLEAR(g_sensor_mode, 3);
            BIT_SET(g_sensor_mode, 4);
        } else if (strncmp(&buf[4], "=100", 4) == 0) {
            BIT_CLEAR(g_sensor_mode, 2);
            BIT_CLEAR(g_sensor_mode, 3);
            BIT_SET(g_sensor_mode, 4);
        } else if (strncmp(&buf[4], "=65", 3) == 0) {
            BIT_SET(g_sensor_mode, 2);
            BIT_SET(g_sensor_mode, 3);
            BIT_CLEAR(g_sensor_mode, 4);
        } else if (strncmp(&buf[4], "=50", 3) == 0) {
            BIT_CLEAR(g_sensor_mode, 2);
            BIT_SET(g_sensor_mode, 3);
            BIT_CLEAR(g_sensor_mode, 4);
        } else if (strncmp(&buf[4], "=35", 3) == 0) {
            BIT_SET(g_sensor_mode, 2);
            BIT_CLEAR(g_sensor_mode, 3);
            BIT_CLEAR(g_sensor_mode, 4);
        } else  if (strncmp(&buf[4], "=25", 3) == 0) {
            BIT_CLEAR(g_sensor_mode, 2);
            BIT_CLEAR(g_sensor_mode, 3);
            BIT_CLEAR(g_sensor_mode, 4);
        } else {
            printf("C:NACK\n");
            return;
        }

        printf("C:ACK\n");
        g_update_rate_hz = sp210_read_datarate_from_config();
        return;
    }
}

static void
core1_entry() {
    while (true) {
        check_process_command();

        if (!stdio_usb_connected()) {
            /* stop measurement if we are disconnected */
            gpio_put(LED_PIN, 0);
            g_running = false;
        }

        sleep_ms(5);
    }
}

inline static int
sp210_write(const uint8_t *src, size_t len, bool nostop)
{
    return i2c_write_blocking(SP210_I2C_HWBLOCK, SP210_ADDR, src, len, nostop);
}

inline static int
sp210_read_raw(uint8_t *dst, size_t len, bool nostop)
{
    return i2c_read_blocking(SP210_I2C_HWBLOCK, SP210_ADDR, dst, len, nostop);
}

static void
sp210_initialize_and_zero()
{
    uint8_t buf[2];
    uint8_t rx_bytes[4];

    /* initialize the sensor */
    buf[0] = g_sensor_mode;
    BIT_CLEAR(buf[0], 5);

    buf[1] = 0b00000000;
    sp210_write(buf, 2, false);
    sleep_ms(20);

    /* dummy read */
    sp210_read_raw(rx_bytes, 4, false);

    /* capture zero point */
    BIT_SET(buf[0], 5);
    buf[1] = 0b00000000;
    sp210_write(buf, 2, false);
    sleep_ms(10);

    /* dummy read */
    sp210_read_raw(rx_bytes, 4, false);
}

static void
sp210_transmit_measurement()
{
    uint8_t rx_bytes[4];
    int16_t pressure_raw;
    uint32_t temperature_mK;
    uint64_t timestamp;
    int64_t pressure_uPa;

    timestamp = current_run_timestamp_ms();
    sp210_read_raw(rx_bytes, 4, false);

    pressure_raw = (rx_bytes[0] << 8) | rx_bytes[1];

    /* °C -> mK */
    temperature_mK = (rx_bytes[2] * 1000) + rx_bytes[3] + 273150;

    /* raw pressure -> inH2O -> Pa -> mPa -> µPa */
    pressure_uPa = ((pressure_raw / (0.9 * pow(2, 15))) * 248.843) * 1000.0 * 1000.0;

    /* transmit data as text... eww... (but performant enough, as it turns out) */
    printf("D:%" PRIu64 ";%" PRIu32 ";%" PRId64 "\n", timestamp, temperature_mK, pressure_uPa);

    /* wait a bit before reading the next datapoint */
    sleep_us(((1000 * 1000) / g_update_rate_hz) - 10);
}

static void
sp210_transmit_info()
{
    uint8_t rx_bytes[21] = {0};

    sp210_read_raw(rx_bytes, 20, false);

    printf("I:Model: %s\n", ((char*)rx_bytes) + 4);
    printf("I:Serial: %02hhX%02hhX%02hhX%02hhX\n", rx_bytes[10], rx_bytes[11], rx_bytes[12], rx_bytes[13]);
    printf("I:Build: %s\n", ((char*)rx_bytes) + 14);

    printf("I:Zero Mode: %s\n", BIT_CHECK(g_sensor_mode, 6)? "ztrack" : "standard");
    printf("I:Zero Noise Supression: %s\n", BIT_CHECK(g_sensor_mode, 7)? "yes" : "no");
    printf("I:Rate: %d Hz\n", sp210_read_datarate_from_config());
}

static void
core0_entry()
{
    while (true) {
        if (g_init_sensor) {
            sp210_initialize_and_zero();
            g_init_sensor = false;
        }

        if (g_running) {
            sp210_transmit_measurement();
        } else {
            if (g_send_info_block) {
                g_send_info_block = false;
                sp210_transmit_info();
            }

            /* delay a bit to not needlessly overwork the core */
            sleep_ms(10);
        }
    }
}

int
main()
{
    /* initialize stdio */
    stdio_init_all();

    bi_decl(bi_program_description("SP210 Differential Pressure Sensor"));

    /* initialize pins */
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 1);

    /* initialize I2C to the sensor */
    i2c_init(SP210_I2C_HWBLOCK, SP210_I2C_FREQ);
    gpio_set_function(SP210_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SP210_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SP210_I2C_SDA_PIN);
    gpio_pull_up(SP210_I2C_SCL_PIN);

    /* set defaults */
    g_init_sensor = true;

    sleep_ms(250);
    gpio_put(LED_PIN, 0);

    /* wait for a USB connection */
    while (!stdio_usb_connected()) {
        sleep_ms(250);
    }

    /* launch multicore tasks */
    multicore_launch_core1(core1_entry);
    core0_entry();

    return 0;
}
