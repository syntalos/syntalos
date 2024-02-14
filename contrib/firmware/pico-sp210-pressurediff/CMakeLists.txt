cmake_minimum_required(VERSION 3.6)

include(pico_sdk_import.cmake)
project(SP210PicoPressureDiff)

pico_sdk_init()

set(TPGEN_TARGET pico-sp210-test)
add_executable(${TPGEN_TARGET}
    pico-sp210.c
)

target_link_libraries(${TPGEN_TARGET}
    pico_stdlib
    pico_multicore
    hardware_i2c
)

target_compile_definitions(${TPGEN_TARGET} PUBLIC
    USBD_PRODUCT="Pico SP210 Sensor"
)

# enable usb output, disable uart output
pico_enable_stdio_usb(${TPGEN_TARGET} 1)
pico_enable_stdio_uart(${TPGEN_TARGET} 0)

# create map/bin/hex file etc.
pico_add_extra_outputs(${TPGEN_TARGET})
