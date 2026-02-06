cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(hid_touch C CXX ASM)

pico_sdk_init()

add_executable(hid_touch
    main.c
)

target_link_libraries(hid_touch
    pico_stdlib
    tinyusb_device
    tinyusb_board
)

pico_add_extra_outputs(hid_touch)
