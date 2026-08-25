/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ZephyrRuntime.h"
#include "ZephyrBluetooth.h"

#include <Arduino.h>

#include <cstring>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>

namespace
{
constexpr size_t deviceIdSize = 16;

int readDeviceId(uint8_t *destination, size_t size)
{
    const int length = hwinfo_get_device_id(destination, size);
    if (length <= 0)
        printk("[zephyr-runtime] hardware device ID unavailable: %d\n", length);
    return length;
}
} // namespace

extern ZephyrBluetooth *zephyrBluetooth;

void getMacAddr(uint8_t *destination)
{
    uint8_t deviceId[deviceIdSize]{};
    const int length = readDeviceId(deviceId, sizeof(deviceId));
    if (length < 6)
        k_panic();

    memcpy(destination, deviceId, 6);
    destination[0] = static_cast<uint8_t>((destination[0] | 0x02U) & 0xFEU);
}

bool getDeviceId(uint8_t *destination)
{
    return readDeviceId(destination, deviceIdSize) > 0;
}

void setBluetoothEnable(bool enable)
{
    if (enable) {
        if (zephyrBluetooth == nullptr) {
            zephyrBluetooth = new ZephyrBluetooth();
            zephyrBluetooth->startDisabled();
        }
        zephyrBluetooth->resumeAdvertising();
    } else if (zephyrBluetooth != nullptr) {
        zephyrBluetooth->shutdown();
    }
}

void clearBonds()
{
    if (zephyrBluetooth == nullptr) {
        zephyrBluetooth = new ZephyrBluetooth();
        zephyrBluetooth->setup();
    }
    zephyrBluetooth->clearBonds();
}

__attribute__((weak)) void enterDfuMode()
{
    printk("[zephyr-runtime] DFU request falling back to a cold reboot\n");
    sys_reboot(SYS_REBOOT_COLD);
}

void cpuDeepSleep(uint32_t milliseconds)
{
    if (milliseconds != UINT32_MAX)
        k_sleep(K_MSEC(milliseconds));
    sys_reboot(SYS_REBOOT_COLD);
}

bool loopCanSleep()
{
    return true;
}

void updateBatteryLevel(uint8_t) {}
