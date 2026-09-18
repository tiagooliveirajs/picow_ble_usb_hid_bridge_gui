cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PICO_BOARD pico2_w CACHE STRING "Board type")
if(NOT PICO_BOARD STREQUAL "pico2_w")
    message(FATAL_ERROR
        "This POC targets Raspberry Pi Pico 2 W (RP2350). "
        "Use -DPICO_BOARD=pico2_w (got '${PICO_BOARD}').")
endif()

set(BRIDGE_COMMON_DIR ${CMAKE_CURRENT_LIST_DIR}/../picow_ble_usb_hid_bridge)
include(${BRIDGE_COMMON_DIR}/pico_sdk_import.cmake)

project(bkb3g_classic_hid_cdc C CXX ASM)

pico_sdk_init()

add_executable(bkb3g_classic_hid_cdc
    main.c
    classic_hid.c
    keyboard_queue.c
    poc_log.c
    usb_descriptors.c
)

target_link_libraries(bkb3g_classic_hid_cdc PRIVATE
    pico_stdlib
    pico_multicore
    pico_flash
    hardware_sync
    pico_unique_id
    tinyusb_device
    tinyusb_board
    pico_btstack_cyw43
    pico_btstack_classic
    pico_cyw43_arch_threadsafe_background
)

target_include_directories(bkb3g_classic_hid_cdc PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_compile_options(bkb3g_classic_hid_cdc PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
)

target_compile_definitions(bkb3g_classic_hid_cdc PRIVATE
    CYW43_LWIP=0
    # Core 1 receives an explicit 8 KiB stack from main.c.
    PICO_CORE1_STACK_SIZE=0
)

# TinyUSB owns USB as a composite CDC ACM + HID device.
# Do not enable Pico SDK USB stdio or UART stdio for this POC.
pico_enable_stdio_usb(bkb3g_classic_hid_cdc 0)
pico_enable_stdio_uart(bkb3g_classic_hid_cdc 0)

pico_add_extra_outputs(bkb3g_classic_hid_cdc)
