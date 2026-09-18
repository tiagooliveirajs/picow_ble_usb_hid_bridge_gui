cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PICO_BOARD pico2_w CACHE STRING "Board type")
if(NOT PICO_BOARD STREQUAL "pico2_w")
    message(FATAL_ERROR "FORK production target is fixed to Raspberry Pi Pico 2 W (got '${PICO_BOARD}').")
endif()

set(BRIDGE_COMMON_DIR ${CMAKE_CURRENT_LIST_DIR}/../picow_ble_usb_hid_bridge)
include(${BRIDGE_COMMON_DIR}/pico_sdk_import.cmake)

project(blu2usb_fork_foundation C CXX ASM)
pico_sdk_init()

add_executable(blu2usb_fork_foundation
    main.c
    bridge_bus.c
    bt_runtime.c
    classic_keyboard.c
    classic_keyboard_report.c
    storage_owner.c
    usb_hid.c
    usb_descriptors.c
)

target_include_directories(blu2usb_fork_foundation PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(blu2usb_fork_foundation PRIVATE
    pico_stdlib
    pico_multicore
    pico_flash
    hardware_sync
    tinyusb_device
    tinyusb_board
    pico_btstack_cyw43
    pico_btstack_ble
    pico_btstack_classic
    pico_cyw43_arch_threadsafe_background
)

target_compile_options(blu2usb_fork_foundation PRIVATE
    -Wall
    -Wextra
)

target_compile_definitions(blu2usb_fork_foundation PRIVATE
    CYW43_LWIP=0
)

# One explicit application-owned 8 KiB Core1 stack is launched from main.c.
# Classic profile code is an adapter inside the single Core1 runtime; it must
# never initialize a second CYW43/BTstack lifecycle.

# Production operation has no diagnostic CDC, UART dependency or debug USB PID.
pico_enable_stdio_usb(blu2usb_fork_foundation 0)
pico_enable_stdio_uart(blu2usb_fork_foundation 0)

pico_add_extra_outputs(blu2usb_fork_foundation)
