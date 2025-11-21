cmake_minimum_required(VERSION 3.12)


set(PICO_BOARD pico2)
set(PICO_COMPILER pico_arm_cortex_m33_gcc)
include(pico_sdk_import.cmake)
include(pico_extras_import_optional.cmake)

pico_sdk_init()

project(Pico_ePaper_Code)


#添加编译子目录
add_subdirectory(lib/audio_data)
add_subdirectory(lib/audio_pio)


#添加头文件目录
include_directories(./lib/audio_data)
include_directories(./lib/audio_pio)


add_executable(audio_firmware
	lib/audio_pio/audio_pio.c
	lib/audio_data/audio_data.c
	main.c
)


pico_generate_pio_header(audio_firmware ${CMAKE_CURRENT_LIST_DIR}/audio_pio.pio)


target_link_libraries(audio_firmware
	audio_pio
	audio_data
	pico_stdlib
	hardware_spi
	hardware_i2c
	hardware_pwm
	hardware_adc
	hardware_pio
	hardware_dma
)


if (PICO_CYW43_SUPPORTED)
	target_link_libraries(blink pico_cyw43_arch_none)
endif()


# enable usb output, disable uart output
pico_enable_stdio_usb(audio_firmware 1)
pico_enable_stdio_uart(audio_firmware 1)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(audio_firmware)


