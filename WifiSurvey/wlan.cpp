#include "wlan.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <thread>

#include <Windows.h>
#include <wlanapi.h>

#pragma comment(lib, "wlanapi.lib")

using namespace std;

namespace {
    VOID WINAPI wlan_session_notify_callback(PWLAN_NOTIFICATION_DATA data, PVOID context) {
        auto scan_done = (timed_mutex*)context;
        if (data->NotificationSource == WLAN_NOTIFICATION_SOURCE_ACM &&
            data->NotificationCode == wlan_notification_acm_scan_complete) {
            scan_done->unlock();
        }
    }

    class wlan_session {
    public:
        // Initializes a new "session" of dealing with the windows wireless API.
        wlan_session() {
            DWORD negotiatedVersion;
            auto ret = WlanOpenHandle(2, nullptr, &negotiatedVersion, &this->handle);
            if (ret != ERROR_SUCCESS) {
                throw new wifi_survey::wlan_exception("WlanOpenHandle", ret);
            }
        }

        ~wlan_session() {
            WlanCloseHandle(this->handle, nullptr);
        }

        vector<WLAN_INTERFACE_INFO> get_interfaces() const {
            PWLAN_INTERFACE_INFO_LIST interfaces;
            auto ret = WlanEnumInterfaces(this->handle, nullptr, &interfaces);
            if (ret != ERROR_SUCCESS) {
                throw new wifi_survey::wlan_exception("WlanEnumInterfaces", ret);
            }

            vector<WLAN_INTERFACE_INFO> out;

            for (DWORD i = 0; i < interfaces->dwNumberOfItems; ++i) {
                auto info = interfaces->InterfaceInfo[i];
                if (info.isState != wlan_interface_state_not_ready) {
                    out.push_back(info);
                }
            }

            WlanFreeMemory(interfaces);
            return out;
        }

        vector<wifi_survey::network> get_available_networks(GUID interfaceId) const {
            timed_mutex scan_done;
            scan_done.lock();

            // register for "scan complete" notifications, then request a scan
            WlanRegisterNotification(this->handle, WLAN_NOTIFICATION_SOURCE_ACM, FALSE, wlan_session_notify_callback, &scan_done, nullptr, nullptr);
            WlanScan(this->handle, &interfaceId, nullptr, nullptr, nullptr);

            // callback will unlock the mutex
            if (!scan_done.try_lock_for(5s)) {
                throw new runtime_error("Scan for wireless networks failed to complete in 5s.");
            }

            // unregister notifications
            WlanRegisterNotification(this->handle, WLAN_NOTIFICATION_SOURCE_NONE, FALSE, nullptr, nullptr, nullptr, nullptr);

            PWLAN_BSS_LIST bss_list;

            auto ret = WlanGetNetworkBssList(
                this->handle,
                &interfaceId,
                nullptr,
                dot11_BSS_type_any,
                FALSE,
                nullptr,
                &bss_list);

            if (ret != ERROR_SUCCESS) {
                throw new wifi_survey::wlan_exception("WlanGetNetworkBssList", ret);
            }

            vector<wifi_survey::network> result;
            for (DWORD i = 0; i < bss_list->dwNumberOfItems; ++i) {
                auto item = bss_list->wlanBssEntries[i];
                std::string ssid(item.dot11Ssid.ucSSID, item.dot11Ssid.ucSSID + item.dot11Ssid.uSSIDLength);

                wifi_survey::network n{ ssid, item.ulChCenterFrequency, item.lRssi };
                result.push_back(n);
            }

            WlanFreeMemory(bss_list);
            return result;
        }

    private:
        HANDLE handle;
    };

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
    vector<network> enumerate_networks() {
        wlan_session session{};

        auto device = session.get_interfaces().front();

        auto networks = session.get_available_networks(device.InterfaceGuid);

        return networks;
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