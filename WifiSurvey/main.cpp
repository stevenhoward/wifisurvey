#include "wlan.h"

#include <algorithm>
#include <iostream>

using namespace std;

int main(int argc, char** argv) {
    auto networks = wifi_survey::enumerate_networks();

    sort(networks.begin(), networks.end(), [](const wifi_survey::network& a, const wifi_survey::network& b) {
        return a.frequency < b.frequency;
    });

    cout << "frequency\tstrength\tname\n";
    for (const auto& network : networks) {
        cout << network.frequency << "\t\t" << network.strength << "dBm\t" << network.name << '\n';
    }

    return 0;
}