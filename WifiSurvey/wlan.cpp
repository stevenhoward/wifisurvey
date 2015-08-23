#include "wlan.h"

#include <memory>

#include <Windows.h>
#include <wlanapi.h>

#pragma comment(lib, "wlanapi.lib")

using namespace std;

namespace {
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
}

namespace wifi_survey {
    vector<network> enumerate_networks() {
        wlan_session session{};

        auto device = session.get_interfaces().front();

        auto networks = session.get_available_networks(device.InterfaceGuid);

        return networks;
    }
}