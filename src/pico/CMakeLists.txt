cmake_minimum_required(VERSION 3.25)


project(VAGC-pico)


set(PICO_BOARD pico2)

include(pico_sdk_import.cmake)
include(pico_extras_import_optional.cmake)

pico_sdk_init()

add_executable(blink
  dsky_output_handler.c
  main.c
  ../core/agc_simulator.c
  ../core/agc_engine_init.c
  ../core/agc_engine.c
  ../core/agc_io_handler.c
  ../core/ringbuffer.c
  ../core/dsky.c
  ../core/profile.c
)

# pull in common dependencies
target_link_libraries(blink pico_stdlib cjson)

if (PICO_CYW43_SUPPORTED)
  target_link_libraries(blink pico_cyw43_arch_none)
endif()

include_directories(..)



# create map/bin/hex file etc.
pico_add_extra_outputs(blink)
pico_enable_stdio_usb(blink 1)
pico_enable_stdio_uart(blink 0)
