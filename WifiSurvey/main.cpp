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
    // ex: table t{ "Name", "Height", "Weight" };
    table(initializer_list<string> headings) {
        contents.push_back(headings);
    }

    // ex.: table.add({ "Jim", "6'0\"", "185lbs" });
    void add(table_row row) {
        contents.push_back(row.contents);
    }

private:
    vector<vector<string>> contents;
    friend ostream& operator<<(ostream& out, const table& table);
};

vector<size_t> get_column_lengths(const vector<vector<string>>& contents) {
    vector<size_t> column_lengths;

    for (size_t i = 0; i < contents[0].size(); ++i) {
        auto longest = max_element(begin(contents), end(contents), [=](const auto& a, const auto& b) {
            return a.at(i).length() < b.at(i).length();
        });

        size_t max_length = longest->at(i).length();
        column_lengths.push_back(max_length);
    }

    return column_lengths;
}

// Makes the table go cout << table and work its glorious magic
ostream& operator<<(ostream& out, const table& table) {
    auto column_lengths = get_column_lengths(table.contents);

    for (const auto& row : table.contents) {
        for (size_t i = 0; i < column_lengths.size(); ++i) {
            out << left << setw(column_lengths[i] + 2) << row[i];
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
    cout << "Adapters:\n";

    int adapterIndex = 0;
    for (auto adapter : adapters) {
        cout << adapterIndex++ << '\t' << adapter.name << '\n';
    }

    if (adapters.size() > 1) {
        while (adapterIndex < 0 || adapterIndex >= adapters.size()) {
            cout << "Enter the number of the adapter to scan on: ";
            cin >> adapterIndex;
        }
    }
    else {
        cout << "Only one adapter.  Scanning.\n";
        adapterIndex = 0;
    }

    auto chosen_adapter = adapters[adapterIndex];
    auto networks = session.enumerate_networks(chosen_adapter);

    sort(begin(networks), end(networks), [](const auto& a, const auto& b) {
        if (a.frequency == b.frequency) {
            if (a.strength == b.strength) {
                return a.name < b.name;
            }

            return b.strength < a.strength;
        }

        return a.frequency < b.frequency;
    });

    table out{ "band", "channel", "strength", "SSID" };

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