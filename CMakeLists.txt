cmake_minimum_required(VERSION 3.20.0)
# Top-level CMakeLists.txt for the skeleton application.
#
# Copyright (c) 2017 Open Source Foundries Limited
#
# SPDX-License-Identifier: Apache-2.0
#
# This provides a basic application structure suitable for communication using
# mcumgr.  It can be used as a starting point for new applications.

# Standard Zephyr application boilerplate.
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(smp_svr)

target_sources(app PRIVATE src/main.c)
target_sources_ifdef(CONFIG_MCUMGR_TRANSPORT_BT app PRIVATE src/bluetooth.c)
target_sources(app PRIVATE src/dfu_hooks.c)

if(DEFINED WEST_TOPDIR)
  set(mcuboot_bootutil_inc ${WEST_TOPDIR}/bootloader/mcuboot/boot/bootutil/include)
  set(mcuboot_zephyr_inc ${WEST_TOPDIR}/bootloader/mcuboot/boot/zephyr/include)
else()
  get_filename_component(west_topdir_fallback "${ZEPHYR_BASE}/.." ABSOLUTE)
  set(mcuboot_bootutil_inc ${west_topdir_fallback}/bootloader/mcuboot/boot/bootutil/include)
  set(mcuboot_zephyr_inc ${west_topdir_fallback}/bootloader/mcuboot/boot/zephyr/include)
endif()

target_include_directories(app PRIVATE ${mcuboot_bootutil_inc} ${mcuboot_zephyr_inc})

if(CONFIG_MCUMGR_TRANSPORT_UDP_DTLS)
  # Use dummy certificate files
  set(cert_dir certificates)
  set(cert_files echo-apps-cert.der;echo-apps-key.der)
  set(gen_dir ${ZEPHYR_BINARY_DIR}/include/generated)

  message(WARNING "Using dummy certificate files, these are provided for demonstration only")

  foreach(inc_file ${cert_files})
    generate_inc_file_for_target(
      app
      ${cert_dir}/${inc_file}
      ${gen_dir}/${inc_file}.inc
    )
  endforeach()

  target_sources(app PRIVATE src/udp_dtls.c)
endif()
