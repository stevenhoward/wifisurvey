#include "wlan.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <thread>

#include <Windows.h>
#include <wlanapi.h>

#pragma comment(lib, "wlanapi.lib")

using namespace std;

// Wrap the exceedingly tedious error handling style C requires and throw an exception.
// Usage: WinFoo(...args) -> TRY_OR_THROW(WinFoo, ...args)
#define TRY_OR_THROW(function, ...) {\
    auto return_code = function(__VA_ARGS__); \
    if (return_code != ERROR_SUCCESS) { \
        throw new wifi_survey::wlan_exception(#function, return_code); \
    } \
}

namespace {
    // tediously ripped from wikipedia
    wifi_survey::frequency_channel_map maps[] = {
        { 1, 2412 },
        { 2, 2417 },
        { 3, 2422 },
        { 4, 2427 },
        { 5, 2432 },
        { 6, 2437 },
        { 7, 2442 },
        { 8, 2447 },
        { 9, 2452 },
        { 10, 2457 },
        { 11, 2462 },
        { 12, 2467 },
        { 13, 2472 },
        { 14, 2484 },

        { 36, 5180 },
        { 40, 5200 },
        { 44, 5220 },
        { 48, 5240 },
        { 52, 5260 },
        { 56, 5280 },
        { 60, 5300 },
        { 64, 5320 },
        { 100, 5500 },
        { 104, 5520 },
        { 108, 5540 },
        { 112, 5560 },
        { 120, 5600 },
        { 124, 5620 },
        { 126, 5640 },
        { 132, 5660 },
        { 136, 5680 },
        { 140, 5700 },
        { 144, 5720 },
        { 149, 5745 },
        { 153, 5765 },
        { 157, 5785 },
        { 161, 5805 },
        { 165, 5825 }
    };
}

namespace wifi_survey {
    VOID WINAPI wlan_session_notify_callback(PWLAN_NOTIFICATION_DATA data, PVOID context) {
        auto scan_done = (timed_mutex*)context;
        if (data->NotificationSource == WLAN_NOTIFICATION_SOURCE_ACM &&
            data->NotificationCode == wlan_notification_acm_scan_complete) {
            scan_done->unlock();
        }
    }

    template<typename T>
    class wlan_scope_guard {
        unique_ptr<T, void(*)(T*)> ptr;

    public:
        wlan_scope_guard(T* t) : ptr(t, [](T* t2) { WlanFreeMemory(t2); }) { }
    };

    // Initializes a new "session" of dealing with the windows wireless API.
    wlan_session::wlan_session() {
        DWORD negotiatedVersion;
        TRY_OR_THROW(WlanOpenHandle, 2, nullptr, &negotiatedVersion, &this->handle);
    }

    wlan_session::~wlan_session() {
        // Cannot throw exceptions in a dtor.
        WlanCloseHandle(this->handle, nullptr);
    }

    vector<wifi_survey::adapter> wlan_session::enumerate_adapters() const {
        PWLAN_INTERFACE_INFO_LIST interfaces;
        TRY_OR_THROW(WlanEnumInterfaces, this->handle, nullptr, &interfaces);

        wlan_scope_guard<WLAN_INTERFACE_INFO_LIST> interfaces_guard(interfaces);

        vector<wifi_survey::adapter> out;

        for (DWORD i = 0; i < interfaces->dwNumberOfItems; ++i) {
            auto info = interfaces->InterfaceInfo[i];
            if (info.isState != wlan_interface_state_not_ready) {
                wstring interface_description(info.strInterfaceDescription);
                string interface_description_str(begin(interface_description), end(interface_description));
                out.push_back(wifi_survey::adapter{ interface_description_str, info.InterfaceGuid });
            }
        }

        return out;
    }

    vector<network> wlan_session::enumerate_networks(adapter adapter) const {
        this->scan_networks_blocking(adapter);

        PWLAN_BSS_LIST bss_list;
        TRY_OR_THROW(
            WlanGetNetworkBssList,
            this->handle,
            &adapter.GUID,
            nullptr,
            dot11_BSS_type_any,
            FALSE,
            nullptr,
            &bss_list);
        wlan_scope_guard<WLAN_BSS_LIST> bss_list_guard(bss_list);

        vector<wifi_survey::network> result;
        for (DWORD i = 0; i < bss_list->dwNumberOfItems; ++i) {
            auto item = bss_list->wlanBssEntries[i];
            std::string ssid(item.dot11Ssid.ucSSID, item.dot11Ssid.ucSSID + item.dot11Ssid.uSSIDLength);

            wifi_survey::network n{ ssid, item.ulChCenterFrequency, item.lRssi };
            result.push_back(n);
        }

        return result;
    }

    void wlan_session::scan_networks_blocking(adapter adapter) const {
        timed_mutex scan_done;
        scan_done.lock();

        // register for "scan complete" notifications, then request a scan
        TRY_OR_THROW(
            WlanRegisterNotification,
            this->handle,
            WLAN_NOTIFICATION_SOURCE_ACM,
            FALSE,
            wlan_session_notify_callback,
            &scan_done,
            nullptr,
            nullptr);

        TRY_OR_THROW(WlanScan, this->handle, &adapter.GUID, nullptr, nullptr, nullptr);

        // callback will unlock the mutex.
        // Scan is required to complete in 4s, per spec, and I don't feel like trying to get fancier.
        if (!scan_done.try_lock_for(5s)) {
            throw new runtime_error("Scan for wireless networks failed to complete in 5s.");
        }

        // unregister notifications
        TRY_OR_THROW(
            WlanRegisterNotification,
            this->handle,
            WLAN_NOTIFICATION_SOURCE_NONE,
            FALSE,
            nullptr,
            nullptr,
            nullptr,
            nullptr);
    }
    
    frequency_channel_map get_frequency_channel_map(unsigned long frequency_khz) {
        auto entry = find_if(begin(maps), end(maps), [=](frequency_channel_map m) {
            return m.frequency_mhz == frequency_khz / 1000;
        });

        if (entry != end(maps)) {
            return *entry;
        }

        throw new runtime_error("Frequency " + to_string(frequency_khz) + " is not a known 802.11 channel");
    }
}