#include "wlan.h"

#include <algorithm>
#include <iomanip>
#include <iostream>

using namespace std;

struct table_row {
    table_row(initializer_list<string> contents_) : contents(contents_) { }
    vector<string> contents;
};

struct table {
    table(initializer_list<string> headings) {
        contents.push_back(headings);
    }

    void add(table_row row) {
        contents.push_back(row.contents);
    }

private:
    vector<vector<string>> contents;
    friend ostream& operator<<(ostream& out, const table& table);
};

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
            out << left  << setw(column_lengths[i] + 1) << row[i];
        }

        out << '\n';
    }

    return out;
}

void run() {
    auto networks = wifi_survey::enumerate_networks();

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
        out.add({ map.band, to_string(map.channel), to_string(network.strength), network.name });
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