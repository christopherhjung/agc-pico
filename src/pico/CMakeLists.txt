cmake_minimum_required(VERSION 3.25)



set(PICO_BOARD pico2)
set(PICO_COMPILER pico_arm_cortex_m33_gcc)
include(pico_sdk_import.cmake)
include(pico_extras_import_optional.cmake)

pico_sdk_init()

project(VAGC)


add_library(cjson STATIC ../../thirdparty/cJSON/cJSON.c)
target_include_directories(cjson PUBLIC ../../thirdparty/)


include_directories(..)

add_executable(agc_pico
  dsky_output_handler.c
  main.c
  times.c
  ../core/agc_simulator.c
  ../core/dsky_dump.c
  ../core/agc_engine_init.c
  ../core/agc_engine.c
  ../core/agc_io_handler.c
  ../core/ringbuffer.c
  ../core/dsky.c
  ../core/profile.c
)

pico_generate_pio_header(agc_pico ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)


# pull in common dependencies
target_link_libraries(agc_pico pico_stdlib hardware_spi hardware_pio hardware_dma cjson)

if (PICO_CYW43_SUPPORTED)
  target_link_libraries(agc_pico pico_cyw43_arch_none)
endif()


# create map/bin/hex file etc.
pico_add_extra_outputs(agc_pico)
pico_enable_stdio_usb(agc_pico 1)
pico_enable_stdio_uart(agc_pico 0)
