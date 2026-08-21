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
    void setup();
    void shutdown();
    void startDisabled();
    void resumeAdvertising();
    void clearBonds();
    bool isConnected();
    int getRssi();
    void sendLog(const uint8_t *logMessage, size_t length);
};
