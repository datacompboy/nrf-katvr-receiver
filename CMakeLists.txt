cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(nrf-katvr-receiver)

target_sources(app PRIVATE src/main.c)

target_sources(app PRIVATE src/kat_ble.c)
target_sources(app PRIVATE src/kat_ble_pack.c)

target_sources(app PRIVATE src/kat_usb.c)
target_sources(app PRIVATE src/kat_usb_pack.c)
target_sources(app PRIVATE src/kat_usb_serialno.c)
