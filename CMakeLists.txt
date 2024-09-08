cmake_minimum_required(VERSION 3.13)
#
set(BOARD pico_sdk)
# Set the TinyUSB log level
# set(LOG 2)
include(pico_sdk_import.cmake)
#
set(target_proj midi_interface)
project(${target_proj} C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()
#
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/midi_uart_lib)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/usb_midi_host)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/Pico-PIO-USB)
# include_directories(${PICO_TINYUSB_PATH}/hw/mcu/raspberry_pi/Pico-PIO-USB/src)
#
add_executable(${target_proj})
#
target_sources(${target_proj} PRIVATE
 # can use 'tinyusb_pico_pio_usb' library later when pico-sdk is updated
# ${PICO_TINYUSB_PATH}/hw/mcu/raspberry_pi/Pico-PIO-USB/src/pio_usb.h
# ${PICO_TINYUSB_PATH}/src/portable/raspberrypi/pio_usb/dcd_pio_usb.c
# ${PICO_TINYUSB_PATH}/src/portable/raspberrypi/pio_usb/hcd_pio_usb.c
 midi_interface.c
# usb_descriptors.c
 )
#
  message("UART debugging is disabled if you use UART 0 for MIDI")
  pico_enable_stdio_uart(${target_proj} 0)
#
target_include_directories(${target_proj} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
#
target_link_options(${target_proj} PRIVATE -Xlinker --print-memory-usage)
target_compile_options(${target_proj} PRIVATE -Wall -Wextra)
#
# use tinyusb implementation
target_compile_definitions(${target_proj} PRIVATE PIO_USB_USE_TINYUSB)
#
target_link_libraries(${target_proj} midi_uart_lib tinyusb_host tinyusb_device tinyusb_board ring_buffer_lib pico_stdlib pico_pio_usb usb_midi_host_app_driver)
#
pico_add_extra_outputs(${target_proj})
