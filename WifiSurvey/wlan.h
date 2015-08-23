#pragma once

#include <string>
#include <vector>

namespace wifi_survey {
    class wlan_exception : std::runtime_error {
    public:
        wlan_exception(std::string function, int error)
            : runtime_error(function + " returned " + std::to_string(error))
        { }
    };

    struct network {
        std::string name;

        unsigned long frequency;

        long strength;
    };

    std::vector<network> enumerate_networks();

    struct frequency_channel_map {
        std::string band;
        int channel;
        unsigned long frequency_mhz;

        frequency_channel_map(int channel, unsigned long frequency_mhz)
            : channel(channel), frequency_mhz(frequency_mhz) {
            if (frequency_mhz > 5000) {
                this->band = "5GHz";
            }
            else {
                this->band = "2.4GHz";
            }
        }
    };

    frequency_channel_map get_frequency_channel_map(unsigned long frequency_khz);
}