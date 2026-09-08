/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

struct device;

namespace meshtastic::platform::zephyr
{

const device *meshtasticSpiDevice();
const device *meshtasticI2cDevice();
bool meshtasticSpiReady();
bool meshtasticI2cReady();

} // namespace meshtastic::platform::zephyr
