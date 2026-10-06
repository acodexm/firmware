/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ZephyrBuses.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>

#define MESHTASTIC_SPI_NODE DT_ALIAS(meshtastic_spi)
#define MESHTASTIC_I2C_NODE DT_ALIAS(meshtastic_i2c)

#if !DT_NODE_EXISTS(MESHTASTIC_SPI_NODE)
#error "Zephyr Meshtastic platform requires a meshtastic-spi DTS alias"
#endif

#if !DT_NODE_EXISTS(MESHTASTIC_I2C_NODE)
#error "Zephyr Meshtastic platform requires a meshtastic-i2c DTS alias"
#endif

namespace meshtastic::platform::zephyr
{

const device *meshtasticSpiDevice()
{
    return DEVICE_DT_GET(MESHTASTIC_SPI_NODE);
}

const device *meshtasticI2cDevice()
{
    return DEVICE_DT_GET(MESHTASTIC_I2C_NODE);
}

bool meshtasticSpiReady()
{
    return device_is_ready(meshtasticSpiDevice());
}

bool meshtasticI2cReady()
{
    return device_is_ready(meshtasticI2cDevice());
}

} // namespace meshtastic::platform::zephyr
