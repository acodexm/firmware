#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

// Boot-scoped cursor for the common baud passes followed by one rare pass.
// The GPS owner retains the current model-probe step within the selected baud.
struct GPSProbeScan {
    uint8_t baudIndex = 0;
    uint8_t commonPass = 0;
    bool exhausted = false;

    void reset() { *this = {}; }
    bool canEnable() const { return !exhausted; }

    template <size_t CommonCount, size_t RareCount>
    bool selectBaud(const int (&common)[CommonCount], const int (&rare)[RareCount], unsigned commonPasses, bool includeRare,
                    int &baud)
    {
        static_assert(CommonCount <= UINT8_MAX && RareCount <= UINT8_MAX);
        const size_t count = baudCount(CommonCount, RareCount, commonPasses, includeRare);
        if (exhausted || baudIndex >= count) {
            exhausted = true;
            return false;
        }
        baud = commonPass < commonPasses ? common[baudIndex] : rare[baudIndex];
        return true;
    }

    void completeFailedBaud(size_t commonCount, size_t rareCount, unsigned commonPasses, bool includeRare)
    {
        const size_t count = baudCount(commonCount, rareCount, commonPasses, includeRare);
        if (exhausted || baudIndex >= count) {
            exhausted = true;
            return;
        }
        if (++baudIndex == count) {
            baudIndex = 0;
            if (commonPass < commonPasses)
                ++commonPass;
            else
                exhausted = true;
        }
        if (baudCount(commonCount, rareCount, commonPasses, includeRare) == 0)
            exhausted = true;
    }

  private:
    size_t baudCount(size_t commonCount, size_t rareCount, unsigned commonPasses, bool includeRare) const
    {
        if (commonPasses > std::numeric_limits<uint8_t>::max())
            return 0;
        if (commonPass < commonPasses)
            return commonCount;
        return commonPass == commonPasses && includeRare ? rareCount : 0;
    }
};
