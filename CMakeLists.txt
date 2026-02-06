cmake_minimum_required(VERSION 3.13)

# Pico SDK を自動取得する
include(FetchContent)

FetchContent_Declare(
  pico_sdk
  GIT_REPOSITORY https://github.com/raspberrypi/pico-sdk.git
  GIT_TAG master
)

FetchContent_MakeAvailable(pico_sdk)

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
