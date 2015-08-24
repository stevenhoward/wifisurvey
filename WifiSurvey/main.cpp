#include "wlan.h"

#include <algorithm>
#include <iomanip>
#include <iostream>

using namespace std;

// Convenience class to make table.add({ }) work.
struct table_row {
    table_row(initializer_list<string> contents_) : contents(contents_) { }
    vector<string> contents;
};

// Tabular output and whatnot
struct table {
    table(initializer_list<string> headings) {
        contents.push_back(headings);
    }

    // ex.: table.add({ "foo", "bar", "baz" })
    void add(table_row row) {
        contents.push_back(row.contents);
    }

private:
    vector<vector<string>> contents;
    friend ostream& operator<<(ostream& out, const table& table);
};

// Makes the table go cout << table and work its glorious magic
ostream& operator<<(ostream& out, const table& table) {
    vector<size_t> column_lengths;
    for (size_t i = 0; i < table.contents[0].size(); ++i) {
        size_t max_length = 0;
        for (const auto& row : table.contents) {
            max_length = max(max_length, row.at(i).length());
        }

        column_lengths.push_back(max_length);
    }

    for (const auto& row : table.contents) {
        for (size_t i = 0; i < column_lengths.size(); ++i) {
            out << left  << setw(column_lengths[i] + 2) << row[i];
        }

        out << '\n';
    }

    return out;
}

// Get the list of available networks, group them by frequency, strength, and network name, then print them
// in a tabular format.
void run() {
    wifi_survey::wlan_session session;
    auto adapters = session.enumerate_adapters();
    cout << "Adapters (TODO: don't just pick the first):\n";
    for (auto adapter : adapters) {
        cout << adapter.name << '\n';
    }

    auto chosen_adapter = adapters.front();
    auto networks = session.enumerate_networks(chosen_adapter);

    sort(networks.begin(), networks.end(), [](const wifi_survey::network& a, const wifi_survey::network& b) {
        if (a.frequency == b.frequency) {
            if (a.strength == b.strength) {
                return a.name < b.name;
            }

            return b.strength < a.strength;
        }

        return a.frequency < b.frequency;
    });

    table out{ "band", "channel", "strength", "SSID" };

    unsigned long last_freq = 0;
    for (const auto& network : networks) {
        auto map = wifi_survey::get_frequency_channel_map(network.frequency);
        out.add({ map.band, to_string(map.channel), to_string(network.strength) + "dBm", network.name });
    }

    cout << out;
}

int main(int argc, char** argv) {
    try {
        run();
    }
    catch (const exception& ex) {
        cerr << "Fatal exception: " << ex.what() << "\n";
        return 1;
    }
}