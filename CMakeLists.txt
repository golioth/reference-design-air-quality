# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(air_quality)

zephyr_include_directories(src/sensirion/common)
zephyr_include_directories(src/sensirion/i2c)
zephyr_include_directories(src/sensirion/scd4x)
zephyr_include_directories(src/sensirion/sps30)

target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/app_rpc.c)
target_sources(app PRIVATE src/app_settings.c)
target_sources(app PRIVATE src/app_state.c)
target_sources(app PRIVATE src/app_work.c)
target_sources(app PRIVATE src/dfu/app_dfu.c)
target_sources(app PRIVATE src/libostentus/libostentus.c)
target_sources_ifdef(CONFIG_BOOTLOADER_MCUBOOT app PRIVATE src/dfu/flash.c)
target_sources(app PRIVATE src/sensirion/common/sensirion_common.c)
target_sources(app PRIVATE src/sensirion/i2c/sensirion_i2c_hal.c)
target_sources(app PRIVATE src/sensirion/i2c/sensirion_i2c.c)
target_sources(app PRIVATE src/sensirion/scd4x/scd4x_i2c.c)
target_sources(app PRIVATE src/sensirion/sps30/sps30_i2c.c)
