#pragma once

#include "../freertosinc.h"
#include "Print.h"
#include "mesh/generated/meshtastic/mesh.pb.h"
#include <stdarg.h>

/**
 * A Printable that can be switched to squirt its bytes to a different sink.
 * This class is mostly useful to allow debug printing to be redirected away from Serial
 * to some other transport if we switch Serial usage (on the fly) to some other purpose.
 */
class RedirectablePrint : public Print
{
    Print *dest;

#ifdef HAS_FREE_RTOS
    SemaphoreHandle_t inDebugPrint = nullptr;
    StaticSemaphore_t _MutexStorageSpace;
#else
    volatile bool inDebugPrint = false;
#endif
  public:
    explicit RedirectablePrint(Print *_dest) : dest(_dest) {}

    using Print::write;

    /**
     * Set a new destination
     */
    void rpInit();
    void setDestination(Print *dest);

    virtual size_t write(uint8_t c);
    size_t write(const uint8_t *buffer, size_t size) override;

    /// Emit one log record to enabled sinks; serial records end with a newline.
    void log(const char *logLevel, const char *format, ...) __attribute__((format(printf, 3, 4)));

    /** like printf but va_list based */
    size_t vprintf(const char *logLevel, const char *format, va_list arg);

    void hexDump(const char *logLevel, const unsigned char *buf, uint16_t len);

  protected:
    /// Subclasses can override if they need to change how we format over the serial port
    virtual void log_to_serial(const char *logLevel, const char *format, va_list arg);
    meshtastic_LogRecord_Level getLogLevel(const char *logLevel);

  private:
    void log_to_syslog(const char *logLevel, const char *format, va_list arg);
    void log_to_ble(const char *logLevel, const char *format, va_list arg);
};
