cmake_minimum_required(VERSION 3.25)



set(PICO_BOARD pico2)
set(PICO_COMPILER pico_arm_cortex_m33_gcc)
include(pico_sdk_import.cmake)
include(pico_extras_import_optional.cmake)

project(VAGC C CXX ASM)

pico_sdk_init()

add_subdirectory(src/pico)
add_subdirectory(thirdparty/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico/src)
