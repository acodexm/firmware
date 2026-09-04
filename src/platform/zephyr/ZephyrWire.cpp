/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "Wire.h"
#include "drivers/ZephyrBuses.h"

#include <cerrno>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>

namespace
{

const device *i2cDevice()
{
    return meshtastic::platform::zephyr::meshtasticI2cDevice();
}

bool i2cReady()
{
    return meshtastic::platform::zephyr::meshtasticI2cReady();
}

void reportI2cFailure(const char *operation, int error)
{
    static uint8_t reports;
    if (reports++ < 8U)
        printk("[zephyr-i2c] %s failed: %d\n", operation, error);
}

} // namespace

TwoWire::TwoWire() : txAddr(0), txLen(0), txBuf{}, rxLen(0), rxPos(0), rxBuf{} {}

void TwoWire::begin()
{
    if (!i2cReady())
        reportI2cFailure("begin", -ENODEV);
}

void TwoWire::begin(uint8_t, uint8_t)
{
    begin();
}

void TwoWire::begin(int, int, uint32_t frequency)
{
    begin();
    if (frequency != 0)
        setClock(frequency);
}

void TwoWire::end() {}

void TwoWire::setClock(uint32_t frequency)
{
    if (!i2cReady()) {
        reportI2cFailure("setClock", -ENODEV);
        return;
    }
    uint32_t speed = I2C_SPEED_STANDARD;
    if (frequency >= 1000000U)
        speed = I2C_SPEED_FAST_PLUS;
    else if (frequency >= 400000U)
        speed = I2C_SPEED_FAST;
    const int error = i2c_configure(i2cDevice(), I2C_MODE_CONTROLLER | I2C_SPEED_SET(speed));
    if (error != 0)
        reportI2cFailure("setClock", error);
}

void TwoWire::beginTransmission(uint8_t address)
{
    txAddr = address;
    txLen = 0;
}

size_t TwoWire::write(uint8_t value)
{
    if (txLen == WIRE_BUFFER_LENGTH)
        return 0;
    txBuf[txLen++] = value;
    return 1;
}

size_t TwoWire::write(const uint8_t *data, size_t size)
{
    if (data == nullptr)
        return 0;
    const size_t available = WIRE_BUFFER_LENGTH - txLen;
    const size_t copied = MIN(size, available);
    memcpy(txBuf + txLen, data, copied);
    txLen += copied;
    return copied;
}

uint8_t TwoWire::endTransmission(bool)
{
    if (!i2cReady()) {
        txLen = 0;
        return 4;
    }
    const int error = i2c_write(i2cDevice(), txBuf, txLen, txAddr);
    txLen = 0;
    if (error == 0)
        return 0;
    if (error == -ETIMEDOUT)
        return 5;
    if (error == -EIO || error == -ENXIO)
        return 2;
    reportI2cFailure("write", error);
    return 4;
}

uint8_t TwoWire::requestFrom(uint8_t address, uint8_t quantity, bool)
{
    rxLen = 0;
    rxPos = 0;
    quantity = MIN(quantity, static_cast<uint8_t>(WIRE_BUFFER_LENGTH));
    if (quantity == 0 || !i2cReady()) {
        txLen = 0;
        return 0;
    }

    int error;
    if (txLen != 0) {
        error = i2c_write_read(i2cDevice(), address, txBuf, txLen, rxBuf, quantity);
        txLen = 0;
    } else {
        error = i2c_read(i2cDevice(), rxBuf, quantity, address);
    }
    if (error != 0) {
        reportI2cFailure("read", error);
        return 0;
    }
    rxLen = quantity;
    return quantity;
}

int TwoWire::available()
{
    return rxLen - rxPos;
}

int TwoWire::read()
{
    if (rxPos == rxLen)
        return -1;
    return rxBuf[rxPos++];
}

int TwoWire::peek()
{
    if (rxPos == rxLen)
        return -1;
    return rxBuf[rxPos];
}

size_t TwoWire::readBytes(uint8_t *buffer, size_t size)
{
    if (buffer == nullptr)
        return 0;
    const size_t copied = MIN(size, static_cast<size_t>(available()));
    memcpy(buffer, rxBuf + rxPos, copied);
    rxPos += copied;
    return copied;
}

TwoWire::operator bool() const
{
    return i2cReady();
}
