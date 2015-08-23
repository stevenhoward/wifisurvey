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
}