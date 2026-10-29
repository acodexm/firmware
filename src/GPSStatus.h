#pragma once
#include "NodeDB.h"
#include "Status.h"
#include "configuration.h"
#include <Arduino.h>
#ifdef RUNA_GNSS_GSA_QUALITY
#include "runa/gps/GnssObservation.h"
#endif

namespace meshtastic
{

/// Describes the state of the GPS system.
class GPSStatus : public Status
{

  private:
    CallbackObserver<GPSStatus, const GPSStatus *> statusObserver =
        CallbackObserver<GPSStatus, const GPSStatus *>(this, &GPSStatus::updateStatus);

    bool hasLock = false;     // default to false, until we complete our first read
    bool isConnected = false; // Do we have a GPS we are talking to

    bool isPowerSaving = false; // Are we in power saving state

    meshtastic_Position p = meshtastic_Position_init_default;

    /// Time of last valid GPS fix (millis since boot)
    uint32_t lastFixMillis = 0;
#ifdef RUNA_GNSS_GSA_QUALITY
    runa::gps::GnssObservation observation;
#endif

  public:
    GPSStatus() { statusType = STATUS_TYPE_GPS; }

    // preferred method
    GPSStatus(bool hasLock, bool isConnected, bool isPowerSaving, const meshtastic_Position &pos
#ifdef RUNA_GNSS_GSA_QUALITY
              ,
              const runa::gps::GnssObservation &source = {}
#endif
              )
        : Status()
    {
        this->hasLock = hasLock;
        this->isConnected = isConnected;
        this->isPowerSaving = isPowerSaving;

        // all-in-one struct copy
        this->p = pos;
#ifdef RUNA_GNSS_GSA_QUALITY
        this->observation = source;
#endif
    }

    GPSStatus(const GPSStatus &);
    GPSStatus &operator=(const GPSStatus &);

    void observe(Observable<const GPSStatus *> *source) { statusObserver.observe(source); }

#ifdef RUNA_GNSS_GSA_QUALITY
    const runa::gps::GnssObservation &getObservation() const { return observation; }
#endif

    bool getHasLock() const { return hasLock; }

    bool getIsConnected() const { return isConnected; }

    bool getIsPowerSaving() const { return isPowerSaving; }

    int32_t getLatitude() const
    {
        if (config.position.fixed_position) {
            return localPosition.latitude_i;
        } else {
            return p.latitude_i;
        }
    }

    int32_t getLongitude() const
    {
        if (config.position.fixed_position) {
            return localPosition.longitude_i;
        } else {
            return p.longitude_i;
        }
    }

    int32_t getAltitude() const
    {
        if (config.position.fixed_position) {
            return localPosition.altitude;
        } else {
            return p.altitude;
        }
    }

    uint32_t getDOP() const { return p.PDOP; }

    uint32_t getHeading() const { return p.ground_track; }

    uint32_t getNumSatellites() const { return p.sats_in_view; }

    /// Return millis() when the last GPS fix occurred (0 = never)
    uint32_t getLastFixMillis() const { return lastFixMillis; }

    bool matches(const GPSStatus *newStatus) const
    {
#ifdef GPS_DEBUG
        LOG_DEBUG("GPSStatus.match() new pos@%x to old pos@%x", newStatus->p.timestamp, p.timestamp);
#endif
        return (newStatus->hasLock != hasLock || newStatus->isConnected != isConnected ||
                newStatus->isPowerSaving != isPowerSaving || newStatus->p.latitude_i != p.latitude_i ||
                newStatus->p.longitude_i != p.longitude_i || newStatus->p.altitude != p.altitude ||
                newStatus->p.altitude_hae != p.altitude_hae || newStatus->p.PDOP != p.PDOP ||
                newStatus->p.ground_track != p.ground_track || newStatus->p.ground_speed != p.ground_speed ||
                newStatus->p.sats_in_view != p.sats_in_view);
    }

    int updateStatus(const GPSStatus *newStatus)
    {
        // Only update the status if values have actually changed
        bool isDirty = matches(newStatus);

#ifdef RUNA_GNSS_GSA_QUALITY
        // Quality/course publication and sub-second receiver epochs can share
        // the legacy whole-second timestamp. Only reused source identity with
        // changed geometry violates Runa's immutable-observation contract.
        if (observation.hasPosition && newStatus->observation.hasPosition &&
            observation.generation == newStatus->observation.generation &&
            observation.sequence == newStatus->observation.sequence &&
            (observation.latitudeE7 != newStatus->observation.latitudeE7 ||
             observation.longitudeE7 != newStatus->observation.longitudeE7)) {
            LOG_ERROR("BUG: GNSS observation identity reused for changed coordinates");
        }
#else
        if (isDirty && p.timestamp && (newStatus->p.timestamp == p.timestamp)) {
            // We can NEVER be in two locations at the same time! (also PR #886)
            LOG_ERROR("BUG: Positional timestamp unchanged from prev solution");
        }
#endif

        initialized = true;
        hasLock = newStatus->hasLock;
        isConnected = newStatus->isConnected;

        // A stationary receiver still produces fresh solutions. Keep freshness
        // independent from observer dirtiness so consumers can reject cached fixes
        // without forcing position/UI notifications every second.
#ifdef RUNA_GNSS_GSA_QUALITY
        observation = newStatus->observation;
        if (observation.hasPosition)
            lastFixMillis = observation.positionReceivedAtMs;
#else
        if (hasLock) {
            lastFixMillis = millis();
        }
#endif

        p = newStatus->p;

        if (isDirty) {
            if (hasLock) {
                // In debug logs, identify position by @timestamp:stage (stage 3 = notify)
                LOG_DEBUG("New GPS pos@%x:3 lat=%f lon=%f alt=%d pdop=%lu.%02lu track=%.2f speed=%.2f sats=%d", p.timestamp,
                          p.latitude_i * 1e-7, p.longitude_i * 1e-7, p.altitude, static_cast<unsigned long>(p.PDOP / 100U),
                          static_cast<unsigned long>(p.PDOP % 100U), p.ground_track * 1e-5, p.ground_speed * 1e-2,
                          p.sats_in_view);
            } else {
                LOG_DEBUG("No GPS lock");
            }
            onNewStatus.notifyObservers(this);
        }
        return 0;
    }
};

} // namespace meshtastic

extern meshtastic::GPSStatus *gpsStatus;
