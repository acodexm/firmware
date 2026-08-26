/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#define ARCH_ZEPHYR 1

#if defined(CONFIG_SOC_SERIES_NRF52X)
#define NRF52840_XXAA 1
#endif

#ifndef HAS_BLUETOOTH
#define HAS_BLUETOOTH 1
#endif
#ifndef HAS_SCREEN
#define HAS_SCREEN 0
#endif
#ifndef HAS_WIRE
#define HAS_WIRE 1
#endif
#ifndef HAS_GPS
#define HAS_GPS 0
#endif
#ifndef HAS_BUTTON
#define HAS_BUTTON 0
#endif
#ifndef HAS_TELEMETRY
#define HAS_TELEMETRY 0
#endif
#ifndef HAS_SENSOR
#define HAS_SENSOR 0
#endif
#ifndef HAS_RADIO
#define HAS_RADIO 1
#endif
#ifndef HAS_CPU_SHUTDOWN
#define HAS_CPU_SHUTDOWN 0
#endif

#ifndef AREF_VOLTAGE
#define AREF_VOLTAGE 3.6
#endif
#ifndef BATTERY_SENSE_RESOLUTION_BITS
#define BATTERY_SENSE_RESOLUTION_BITS 12
#endif

#ifndef HW_VENDOR
#define HW_VENDOR meshtastic_HardwareModel_PRIVATE_HW
#endif

#include <zephyr/kernel.h>
#define xPortInIsrContext() k_is_in_isr()

// GPS.cpp uses the Arduino-core name for its selected UART implementation.
#define SerialUART HardwareSerial
