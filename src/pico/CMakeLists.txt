cmake_minimum_required(VERSION 3.25)


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
target_link_libraries(agc_pico
  pico_stdlib
  pico_multicore
  hardware_spi
  hardware_pio
  hardware_dma
  cjson
)

if (PICO_CYW43_SUPPORTED)
  target_link_libraries(agc_pico pico_cyw43_arch_none)
endif()

# create map/bin/hex file etc.
pico_add_extra_outputs(agc_pico)
pico_enable_stdio_usb(agc_pico 1)
pico_enable_stdio_uart(agc_pico 0)


add_executable(my_sd_card
  my_sd_card.c
  hw_config.c
  audio_pio.c
)
target_include_directories(my_sd_card PRIVATE ../../thirdparty/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico/include)

pico_generate_pio_header(my_sd_card ${CMAKE_CURRENT_LIST_DIR}/audio_pio.pio)


# pull in common dependencies
target_link_libraries(my_sd_card
  pico_stdlib
  hardware_spi
  hardware_pio
  hardware_dma
  no-OS-FatFS-SD-SDIO-SPI-RPi-Pico
)

if (PICO_CYW43_SUPPORTED)
  target_link_libraries(my_sd_card pico_cyw43_arch_none)
endif()

pico_add_extra_outputs(my_sd_card)
pico_enable_stdio_usb(my_sd_card 1)
pico_enable_stdio_uart(my_sd_card 0)
#
