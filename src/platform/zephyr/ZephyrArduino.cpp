/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "Arduino.h"
#include "SPI.h"
#include "Wire.h"
#include "drivers/ZephyrBuses.h"

#include <climits>
#include <cstdarg>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>

SPIClass SPI;
SPIClass SPI1;
TwoWire Wire;
TwoWire Wire1;
HardwareSerial Serial;
namespace
{
const device *consoleUartDevice()
{
    return DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
}

const device *gpsUartDevice()
{
#if DT_NODE_HAS_STATUS(DT_ALIAS(gps_uart), okay)
    return DEVICE_DT_GET(DT_ALIAS(gps_uart));
#else
    return nullptr;
#endif
}
} // namespace
HardwareSerial Serial1(gpsUartDevice());
HardwareSerial Serial2;

extern "C" void __attribute__((weak)) _fini(void) {}

extern "C" uint32_t millis(void)
{
    return k_uptime_get_32();
}

extern "C" uint32_t micros(void)
{
    return k_cyc_to_us_floor32(k_cycle_get_32());
}

extern "C" void delay(uint32_t ms)
{
    k_sleep(K_MSEC(ms));
}

extern "C" void delayMicroseconds(uint32_t us)
{
    if (us < 1000U) {
        k_busy_wait(us);
    } else {
        k_sleep(K_USEC(us));
    }
}

extern "C" void yield(void)
{
    k_yield();
}

#pragma push_macro("NVIC_SystemReset")
#undef NVIC_SystemReset
extern "C" void NVIC_SystemReset(void)
{
    sys_reboot(SYS_REBOOT_COLD);
}
#pragma pop_macro("NVIC_SystemReset")

HardwareSerial::HardwareSerial() : HardwareSerial(nullptr) {}

HardwareSerial::HardwareSerial(const device *uartDevice) : uartDevice_(uartDevice)
{
    ring_buf_init(&receiveRing_, sizeof(receiveStorage_), receiveStorage_);
}

void HardwareSerial::begin(unsigned long baud)
{
    begin(baud, SERIAL_8N1);
}

void HardwareSerial::begin(unsigned long baud, uint16_t config)
{
    begin(baud, static_cast<uint32_t>(config), -1, -1, false);
}

void HardwareSerial::begin(unsigned long baud, uint32_t, int8_t, int8_t, bool invert)
{
    if (uartDevice_ == nullptr || invert || !device_is_ready(uartDevice_))
        return;

    if (started_)
        uart_irq_rx_disable(uartDevice_);
    started_ = false;

    uart_config uartConfig{};
    uartConfig.baudrate = static_cast<uint32_t>(baud);
    uartConfig.parity = UART_CFG_PARITY_NONE;
    uartConfig.stop_bits = UART_CFG_STOP_BITS_1;
    uartConfig.data_bits = UART_CFG_DATA_BITS_8;
    uartConfig.flow_ctrl = UART_CFG_FLOW_CTRL_NONE;
    if (uart_configure(uartDevice_, &uartConfig) != 0 || uart_irq_callback_user_data_set(uartDevice_, handleInterrupt, this) != 0)
        return;

    ring_buf_reset(&receiveRing_);
    uart_irq_rx_enable(uartDevice_);
    baud_ = baud;
    started_ = true;
}

void HardwareSerial::end()
{
    if (uartDevice_ != nullptr && started_)
        uart_irq_rx_disable(uartDevice_);
    started_ = false;
    baud_ = 0;
    ring_buf_reset(&receiveRing_);
}

void HardwareSerial::handleInterrupt(const device *uartDevice, void *context)
{
    auto *serial = static_cast<HardwareSerial *>(context);
    if (serial == nullptr || !uart_irq_update(uartDevice))
        return;

    uint8_t bytes[32];
    while (uart_irq_rx_ready(uartDevice)) {
        const int received = uart_fifo_read(uartDevice, bytes, sizeof(bytes));
        if (received <= 0)
            break;
        (void)ring_buf_put(&serial->receiveRing_, bytes, static_cast<uint32_t>(received));
    }
}

int HardwareSerial::available()
{
    return static_cast<int>(ring_buf_size_get(&receiveRing_));
}

int HardwareSerial::read()
{
    uint8_t value = 0;
    return ring_buf_get(&receiveRing_, &value, 1) == 1 ? value : -1;
}

int HardwareSerial::peek()
{
    uint8_t value = 0;
    return ring_buf_peek(&receiveRing_, &value, 1) == 1 ? value : -1;
}

size_t HardwareSerial::readBytes(uint8_t *buffer, size_t size)
{
    if (buffer == nullptr)
        return 0;

    const uint32_t startedAt = k_uptime_get_32();
    size_t readCount = 0;
    while (readCount < size && k_uptime_get_32() - startedAt < timeoutMs_) {
        const uint32_t received = ring_buf_get(&receiveRing_, buffer + readCount, static_cast<uint32_t>(size - readCount));
        readCount += received;
        if (received == 0)
            k_sleep(K_MSEC(1));
    }
    return readCount;
}

HardwareSerial::operator bool() const
{
    return uartDevice_ == nullptr || (started_ && device_is_ready(uartDevice_));
}

void HardwareSerial::flush() {}

size_t HardwareSerial::write(uint8_t value)
{
    if (uartDevice_ != nullptr) {
        if (!started_)
            return 0;
        uart_poll_out(uartDevice_, value);
        return 1;
    }
    const device *console = consoleUartDevice();
    if (device_is_ready(console))
        (void)uart_fifo_fill(console, &value, 1);
    return 1;
}

size_t HardwareSerial::write(const uint8_t *buffer, size_t size)
{
    if (buffer == nullptr)
        return 0;

    if (uartDevice_ != nullptr) {
        if (!started_)
            return 0;
        for (size_t index = 0; index < size; ++index)
            uart_poll_out(uartDevice_, buffer[index]);
        return size;
    }

    const device *console = consoleUartDevice();
    if (device_is_ready(console))
        (void)uart_fifo_fill(console, buffer, static_cast<int>(MIN(size, static_cast<size_t>(INT_MAX))));
    return size;
}

int Print::printf(const char *format, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, format);
    const int result = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (result > 0) {
        const size_t length = MIN(static_cast<size_t>(result), sizeof(buffer) - 1U);
        write(reinterpret_cast<const uint8_t *>(buffer), length);
    }
    return result;
}

extern "C" size_t strlcpy(char *destination, const char *source, size_t size)
{
    const size_t length = strlen(source);
    if (size != 0U) {
        const size_t copied = MIN(length, size - 1U);
        memcpy(destination, source, copied);
        destination[copied] = '\0';
    }
    return length;
}

namespace
{

size_t printNumber(Print &output, unsigned long value, uint8_t base)
{
    if (base == 0)
        return output.write(static_cast<uint8_t>(value));
    if (base < 2 || base > 36)
        return 0;

    char buffer[8 * sizeof(long) + 1];
    char *cursor = buffer + sizeof(buffer) - 1;
    *cursor = '\0';
    do {
        const unsigned long digit = value % base;
        *--cursor = static_cast<char>(digit < 10 ? '0' + digit : 'A' + digit - 10);
        value /= base;
    } while (value != 0);
    return output.write(reinterpret_cast<const uint8_t *>(cursor), strlen(cursor));
}

size_t printFloat(Print &output, double value, uint8_t digits)
{
    if (std::isnan(value))
        return output.print("nan");
    if (std::isinf(value))
        return output.print("inf");
    if (value > 4294967040.0 || value < -4294967040.0)
        return output.print("ovf");

    size_t written = 0;
    if (value < 0.0) {
        written += output.write('-');
        value = -value;
    }
    double rounding = 0.5;
    for (uint8_t i = 0; i < digits; ++i)
        rounding /= 10.0;
    value += rounding;

    const unsigned long integer = static_cast<unsigned long>(value);
    double remainder = value - static_cast<double>(integer);
    written += printNumber(output, integer, 10);
    if (digits != 0) {
        written += output.write('.');
        while (digits-- != 0) {
            remainder *= 10.0;
            const auto digit = static_cast<unsigned int>(remainder);
            written += output.write(static_cast<uint8_t>('0' + digit));
            remainder -= digit;
        }
    }
    return written;
}

struct PinDevice {
    const device *controller;
    gpio_pin_t pin;
};

bool resolvePin(uint32_t arduinoPin, PinDevice &resolved)
{
#if defined(CONFIG_SOC_SERIES_NRF52) || defined(CONFIG_SOC_COMPATIBLE_NRF52X)
    constexpr uint32_t pinsPerPort = 32;
#else
    constexpr uint32_t pinsPerPort = 16;
#endif
#if defined(CONFIG_SOC_NRF52840)
    static_assert(45U / pinsPerPort == 1U, "nRF52840 P1 pins must resolve through GPIO1");
    static_assert(45U % pinsPerPort == 13U, "Arduino pin 45 must resolve to nRF52840 P1.13");
#endif
    const uint32_t port = arduinoPin / pinsPerPort;
    resolved.pin = static_cast<gpio_pin_t>(arduinoPin % pinsPerPort);
    switch (port) {
#if DT_NODE_HAS_STATUS(DT_NODELABEL(gpio0), okay)
    case 0:
        resolved.controller = DEVICE_DT_GET(DT_NODELABEL(gpio0));
        break;
#endif
#if DT_NODE_HAS_STATUS(DT_NODELABEL(gpio1), okay)
    case 1:
        resolved.controller = DEVICE_DT_GET(DT_NODELABEL(gpio1));
        break;
#endif
#if DT_NODE_HAS_STATUS(DT_NODELABEL(gpio2), okay)
    case 2:
        resolved.controller = DEVICE_DT_GET(DT_NODELABEL(gpio2));
        break;
#endif
    default:
        return false;
    }
    return device_is_ready(resolved.controller);
}

constexpr size_t maxInterrupts = 8;

struct PinInterrupt {
    gpio_callback callback;
    voidFuncPtr handler;
    const device *controller;
    gpio_pin_t pin;
    bool used;
};

PinInterrupt interruptEntries[maxInterrupts]{};

void dispatchInterrupt(const device *, gpio_callback *callback, uint32_t)
{
    auto *entry = CONTAINER_OF(callback, PinInterrupt, callback);
    if (entry->handler != nullptr)
        entry->handler();
}

void reportGpioFailure(const char *operation, uint32_t pin, int error)
{
    static uint8_t reports;
    if (reports++ < 8U)
        printk("[zephyr-gpio] %s pin %u failed: %d\n", operation, pin, error);
}

int transceive(const SPISettings &settings, const uint8_t *tx, uint8_t *rx, size_t size)
{
    const device *controller = meshtastic::platform::zephyr::meshtasticSpiDevice();
    if (!device_is_ready(controller))
        return -ENODEV;

    spi_operation_t operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8);
    operation |= settings.bitOrder == LSBFIRST ? SPI_TRANSFER_LSB : SPI_TRANSFER_MSB;
    if (settings.dataMode == SPI_MODE1 || settings.dataMode == SPI_MODE3)
        operation |= SPI_MODE_CPHA;
    if (settings.dataMode == SPI_MODE2 || settings.dataMode == SPI_MODE3)
        operation |= SPI_MODE_CPOL;
    const spi_config config = {
        .frequency = settings.clock,
        .operation = operation,
        .slave = 0,
        .cs = {},
    };

    spi_buf txBuffer = {.buf = const_cast<uint8_t *>(tx), .len = size};
    spi_buf rxBuffer = {.buf = rx, .len = size};
    spi_buf_set txSet = {.buffers = tx != nullptr ? &txBuffer : nullptr, .count = tx != nullptr ? 1U : 0U};
    spi_buf_set rxSet = {.buffers = rx != nullptr ? &rxBuffer : nullptr, .count = rx != nullptr ? 1U : 0U};
    return spi_transceive(controller, &config, tx != nullptr ? &txSet : nullptr, rx != nullptr ? &rxSet : nullptr);
}

void reportSpiFailure(int error)
{
    static uint8_t reports;
    if (reports++ < 8U)
        printk("[zephyr-spi] transfer failed: %d\n", error);
}

} // namespace

void SPIClass::begin()
{
    if (!meshtastic::platform::zephyr::meshtasticSpiReady())
        reportSpiFailure(-ENODEV);
}

void SPIClass::begin(uint8_t, uint8_t, uint8_t, uint8_t)
{
    begin();
}

void SPIClass::end() {}

void SPIClass::beginTransaction(SPISettings requested)
{
    settings = requested;
    begin();
}

void SPIClass::endTransaction() {}

void SPIClass::setBitOrder(uint8_t order)
{
    settings.bitOrder = order;
}

void SPIClass::setDataMode(uint8_t mode)
{
    settings.dataMode = mode;
}

void SPIClass::setClockDivider(uint8_t divider)
{
    if (divider != 0)
        settings.clock = MAX(1U, settings.clock / divider);
}

void SPIClass::setFrequency(uint32_t frequency)
{
    if (frequency != 0)
        settings.clock = frequency;
}

size_t Print::print(unsigned char value, int base)
{
    return printNumber(*this, value, base);
}
size_t Print::print(unsigned int value, int base)
{
    return printNumber(*this, value, base);
}
size_t Print::print(unsigned long value, int base)
{
    return printNumber(*this, value, base);
}
size_t Print::print(float value, int digits)
{
    return printFloat(*this, value, digits);
}
size_t Print::print(double value, int digits)
{
    return printFloat(*this, value, digits);
}

size_t Print::print(int value, int base)
{
    if (base == 10 && value < 0)
        return write('-') + printNumber(*this, static_cast<unsigned long>(-static_cast<long>(value)), base);
    return printNumber(*this, static_cast<unsigned long>(value), base);
}

size_t Print::print(long value, int base)
{
    if (base == 10 && value < 0)
        return write('-') + printNumber(*this, 0UL - static_cast<unsigned long>(value), base);
    return printNumber(*this, static_cast<unsigned long>(value), base);
}

void String::replace(const String &from, const String &to)
{
    if (from.isEmpty() || _buf == nullptr)
        return;
    String result;
    const char *cursor = _buf;
    while (*cursor != '\0') {
        if (strncmp(cursor, from.c_str(), from.length()) == 0) {
            result += to;
            cursor += from.length();
        } else {
            result += *cursor++;
        }
    }
    *this = result;
}

void pinMode(uint32_t pin, uint32_t mode)
{
    PinDevice resolved{};
    if (!resolvePin(pin, resolved)) {
        reportGpioFailure("resolve", pin, -ENODEV);
        return;
    }
    gpio_flags_t flags = GPIO_INPUT;
    switch (mode) {
    case OUTPUT:
        flags = GPIO_OUTPUT_INACTIVE;
        break;
    case OUTPUT_OPENDRAIN:
        flags = GPIO_OUTPUT_INACTIVE | GPIO_OPEN_DRAIN;
        break;
    case INPUT_PULLUP:
        flags = GPIO_INPUT | GPIO_PULL_UP;
        break;
    case INPUT_PULLDOWN:
        flags = GPIO_INPUT | GPIO_PULL_DOWN;
        break;
    default:
        break;
    }
    const int error = gpio_pin_configure(resolved.controller, resolved.pin, flags);
    if (error != 0)
        reportGpioFailure("configure", pin, error);
}

void digitalWrite(uint32_t pin, uint32_t value)
{
    PinDevice resolved{};
    if (!resolvePin(pin, resolved)) {
        reportGpioFailure("resolve", pin, -ENODEV);
        return;
    }
    const int error = gpio_pin_set_raw(resolved.controller, resolved.pin, value == LOW ? 0 : 1);
    if (error != 0)
        reportGpioFailure("write", pin, error);
}

int digitalRead(uint32_t pin)
{
    PinDevice resolved{};
    if (!resolvePin(pin, resolved)) {
        reportGpioFailure("resolve", pin, -ENODEV);
        return LOW;
    }
    const int value = gpio_pin_get_raw(resolved.controller, resolved.pin);
    if (value < 0) {
        reportGpioFailure("read", pin, value);
        return LOW;
    }
    return value;
}

void attachInterrupt(uint32_t pin, voidFuncPtr handler, int mode)
{
    PinDevice resolved{};
    if (!resolvePin(pin, resolved) || handler == nullptr) {
        reportGpioFailure("interrupt resolve", pin, -EINVAL);
        return;
    }

    PinInterrupt *entry = nullptr;
    for (auto &candidate : interruptEntries) {
        if (candidate.used && candidate.controller == resolved.controller && candidate.pin == resolved.pin) {
            gpio_remove_callback(candidate.controller, &candidate.callback);
            entry = &candidate;
            break;
        }
        if (entry == nullptr && !candidate.used)
            entry = &candidate;
    }
    if (entry == nullptr) {
        reportGpioFailure("interrupt table full", pin, -ENOMEM);
        return;
    }

    gpio_flags_t trigger = GPIO_INT_EDGE_BOTH;
    if (mode == RISING)
        trigger = GPIO_INT_EDGE_RISING;
    else if (mode == FALLING)
        trigger = GPIO_INT_EDGE_FALLING;

    entry->handler = handler;
    entry->controller = resolved.controller;
    entry->pin = resolved.pin;
    entry->used = true;
    gpio_init_callback(&entry->callback, dispatchInterrupt, BIT(resolved.pin));
    int error = gpio_add_callback(resolved.controller, &entry->callback);
    if (error == 0)
        error = gpio_pin_interrupt_configure(resolved.controller, resolved.pin, trigger);
    if (error != 0) {
        gpio_remove_callback(resolved.controller, &entry->callback);
        entry->used = false;
        reportGpioFailure("interrupt configure", pin, error);
    }
}

void detachInterrupt(uint32_t pin)
{
    PinDevice resolved{};
    if (!resolvePin(pin, resolved))
        return;
    for (auto &entry : interruptEntries) {
        if (entry.used && entry.controller == resolved.controller && entry.pin == resolved.pin) {
            gpio_pin_interrupt_configure(entry.controller, entry.pin, GPIO_INT_DISABLE);
            gpio_remove_callback(entry.controller, &entry.callback);
            entry.used = false;
            return;
        }
    }
}

uint8_t SPIClass::transfer(uint8_t value)
{
    uint8_t received = 0xFF;
    const int error = transceive(settings, &value, &received, 1);
    if (error != 0)
        reportSpiFailure(error);
    return received;
}

uint16_t SPIClass::transfer16(uint16_t value)
{
    uint8_t sent[] = {static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value)};
    uint8_t received[] = {0xFF, 0xFF};
    const int error = transceive(settings, sent, received, sizeof(sent));
    if (error != 0)
        reportSpiFailure(error);
    return static_cast<uint16_t>((static_cast<uint16_t>(received[0]) << 8) | received[1]);
}

void SPIClass::transferBytes(const uint8_t *tx, uint8_t *rx, uint32_t count)
{
    if (count == 0)
        return;
    const int error = transceive(settings, tx, rx, count);
    if (error != 0)
        reportSpiFailure(error);
}

void SPIClass::transfer(void *buffer, size_t size)
{
    if (buffer != nullptr && size != 0)
        transferBytes(static_cast<const uint8_t *>(buffer), static_cast<uint8_t *>(buffer), size);
}
