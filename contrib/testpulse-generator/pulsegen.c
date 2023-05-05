/*
 * Copyright (C) 2022-2023 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pll.h"
#include "hardware/clocks.h"
#include "hardware/structs/pll.h"
#include "hardware/structs/clocks.h"


#define LED_PIN         22
#define MODE_SWITCH_PIN  5
#define LED_PULSE_LEN 240


static void emit_led_pulse(uint ledPin)
{
    gpio_put(LED_PIN, 1);
    sleep_ms(LED_PULSE_LEN);
    gpio_put(LED_PIN, 0);
}

static void emit_pulse_varsequence(uint ledPin)
{
    emit_led_pulse(ledPin);
    sleep_ms(1000 - LED_PULSE_LEN);
    emit_led_pulse(ledPin);
    sleep_ms(2000 - LED_PULSE_LEN);
    emit_led_pulse(ledPin);
    sleep_ms(3000 - LED_PULSE_LEN);
}

static void emit_pulse_staticsequence(uint ledPin)
{
    emit_led_pulse(ledPin);
    sleep_ms(1000 - LED_PULSE_LEN);
}

int main()
{
    stdio_init_all();

    // Change clk_sys to be 48MHz. The simplest way is to take this from PLL_USB
    // which has a source frequency of 48MHz
    clock_configure(clk_sys,
                    CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
                    CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
                    48 * MHZ,
                    48 * MHZ);

    // Turn off PLL sys for good measure
    pll_deinit(pll_sys);

    // CLK peri is clocked from clk_sys so need to change clk_peri's freq
    clock_configure(clk_peri,
                    0,
                    CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS,
                    48 * MHZ,
                    48 * MHZ);

    // Re init now that clk_peri has changed
    stdio_init_all();

    // we are in 48MHz mode now, for more precise timings

    // Configure all pins
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    // Configure default button to switch modes
    gpio_init(MODE_SWITCH_PIN);
    gpio_set_dir(MODE_SWITCH_PIN, GPIO_IN);
    gpio_set_pulls(MODE_SWITCH_PIN, true, false);
    bool button_state = gpio_get(MODE_SWITCH_PIN);

    // Run
    if (button_state) {
        while (true)
            emit_pulse_varsequence(LED_PIN);
    } else {
        while (true)
            emit_pulse_staticsequence(LED_PIN);
    }

    return 0;
}
