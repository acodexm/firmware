// Shared Zephyr BLE backend for Meshtastic.
//
// GATT profile is identical to the nRF52 implementation:
//   Service:   MESH_SERVICE_UUID
//   toRadio:   TORADIO_UUID   (WRITE)
//   fromRadio: FROMRADIO_UUID (READ)
//   fromNum:   FROMNUM_UUID   (READ | NOTIFY)
//   logRadio:  LOGRADIO_UUID  (READ | NOTIFY | INDICATE)

#pragma once

#include "BluetoothCommon.h"

class ZephyrBluetooth : public BluetoothApi
{
  public:
    struct LogTransportStats {
        uint32_t submitted = 0;
        uint32_t unsubscribed = 0;
        uint32_t disconnected = 0;
        uint32_t invalid = 0;
        uint32_t mtuExceeded = 0;
        uint32_t noBuffers = 0;
        uint32_t notifyErrors = 0;
        int32_t lastNotifyError = 0;
        uint16_t lastMtu = 0;
    };

    void setup();
    void shutdown();
    void startDisabled();
    void resumeAdvertising();
    void clearBonds();
    bool isConnected();
    int getRssi();
    bool isLogSubscribed() const;
    LogTransportStats logTransportStats() const;
    void sendLog(const uint8_t *logMessage, size_t length);
};
