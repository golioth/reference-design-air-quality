# Copyright (c) 2023 Golioth, Inc.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(air_quality)

zephyr_include_directories(external/sensirion/embedded-common/common)
zephyr_include_directories(external/sensirion/embedded-common/i2c)
zephyr_include_directories(external/sensirion/embedded-i2c-scd4x)
zephyr_include_directories(external/sensirion/embedded-sps/sps-common)
zephyr_include_directories(external/sensirion/embedded-sps/sps30-i2c)

target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/app_rpc.c)
target_sources(app PRIVATE src/app_settings.c)
target_sources(app PRIVATE src/app_state.c)
target_sources(app PRIVATE src/app_work.c)
target_sources(app PRIVATE src/dfu/app_dfu.c)
target_sources(app PRIVATE src/libostentus/libostentus.c)
target_sources_ifdef(CONFIG_BOOTLOADER_MCUBOOT app PRIVATE src/dfu/flash.c)
target_sources(app PRIVATE src/sensors.c)
target_sources(app PRIVATE external/sensirion/embedded-common/common/sensirion_common.c)
target_sources(app PRIVATE external/sensirion/embedded-common/i2c/sensirion_i2c_hal.c)
target_sources(app PRIVATE external/sensirion/embedded-common/i2c/sensirion_i2c.c)
target_sources(app PRIVATE external/sensirion/embedded-i2c-scd4x/scd4x_i2c.c)
target_sources(app PRIVATE external/sensirion/embedded-sps/sps30-i2c/sps30.c)

add_subdirectory_ifdef(CONFIG_ALUDEL_BATTERY_MONITOR src/battery_monitor)
