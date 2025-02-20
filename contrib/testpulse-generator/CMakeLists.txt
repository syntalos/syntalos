cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)
project(SyntalosTestpulseGenerator)

pico_sdk_init()

set(TPGEN_TARGET syntalos-testpulse-generator)
add_executable(${TPGEN_TARGET}
    pulsegen.c
)

target_link_libraries(${TPGEN_TARGET}
    pico_stdlib
    hardware_clocks
)

# enable usb output, disable uart output
pico_enable_stdio_usb(${TPGEN_TARGET} 1)
pico_enable_stdio_uart(${TPGEN_TARGET} 0)

# create map/bin/hex file etc.
pico_add_extra_outputs(${TPGEN_TARGET})

# metadata
pico_set_program_url(${TPGEN_TARGET} "https://github.com/syntalos/syntalos/tree/master/contrib/testpulse-generator")
