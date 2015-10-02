/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain
 * this notice you can do whatever you want with this stuff. If we meet some day,
 * and you think this stuff is worth it, you can buy me a beer in return.
 * ----------------------------------------------------------------------------
 * Heavily modified and enhanced by Thorsten von Eicken in 2015
 * ----------------------------------------------------------------------------
 */

#include <esp8266.h>
 
#include "wifi.h"

//#define SLEEP_MODE LIGHT_SLEEP_T
#define SLEEP_MODE MODEM_SLEEP_T
#define DEBUGIP
#define CGIWIFI_DBG

// ===== wifi status change callbacks
static WifiStateChangeCb wifi_state_change_cb[4];

uint8_t wifiState = wifiIsDisconnected;
// reasons for which a connection failed
uint8_t wifiReason = 0;
static char *wifiReasons[] = {
  "", "unspecified", "auth_expire", "auth_leave", "assoc_expire", "assoc_toomany", "not_authed",
  "not_assoced", "assoc_leave", "assoc_not_authed", "disassoc_pwrcap_bad", "disassoc_supchan_bad",
  "ie_invalid", "mic_failure", "4way_handshake_timeout", "group_key_update_timeout",
  "ie_in_4way_differs", "group_cipher_invalid", "pairwise_cipher_invalid", "akmp_invalid",
  "unsupp_rsn_ie_version", "invalid_rsn_ie_cap", "802_1x_auth_failed", "cipher_suite_rejected",
  "beacon_timeout", "no_ap_found" };

static char *wifiMode[] = { 0, "STA", "AP", "AP+STA" };

void (*wifiStatusCb)(uint8_t); // callback when wifi status changes

static char* ICACHE_FLASH_ATTR wifiGetReason(void) {
  if (wifiReason <= 24) return wifiReasons[wifiReason];
  if (wifiReason >= 200 && wifiReason <= 201) return wifiReasons[wifiReason-200+24];
  return wifiReasons[1];
}

// handler for wifi status change callback coming in from espressif library
static void ICACHE_FLASH_ATTR wifiHandleEventCb(System_Event_t *evt) {
  switch (evt->event) {
  case EVENT_STAMODE_CONNECTED:
    wifiState = wifiIsConnected;
    wifiReason = 0;
    os_printf("Wifi connected to ssid %s, ch %d\n", evt->event_info.connected.ssid,
        evt->event_info.connected.channel);
    break;
  case EVENT_STAMODE_DISCONNECTED:
    wifiState = wifiIsDisconnected;
    wifiReason = evt->event_info.disconnected.reason;
    os_printf("Wifi disconnected from ssid %s, reason %s (%d)\n",
        evt->event_info.disconnected.ssid, wifiGetReason(), evt->event_info.disconnected.reason);
    break;
  case EVENT_STAMODE_AUTHMODE_CHANGE:
    os_printf("Wifi auth mode: %d -> %d\n",
        evt->event_info.auth_change.old_mode, evt->event_info.auth_change.new_mode);
    break;
  case EVENT_STAMODE_GOT_IP:
    wifiState = wifiGotIP;
    wifiReason = 0;
    os_printf("Wifi got ip:" IPSTR ",mask:" IPSTR ",gw:" IPSTR "\n",
        IP2STR(&evt->event_info.got_ip.ip), IP2STR(&evt->event_info.got_ip.mask),
        IP2STR(&evt->event_info.got_ip.gw));
    break;
  case EVENT_SOFTAPMODE_STACONNECTED:
    os_printf("Wifi AP: station " MACSTR " joined, AID = %d\n",
        MAC2STR(evt->event_info.sta_connected.mac), evt->event_info.sta_connected.aid);
    break;
  case EVENT_SOFTAPMODE_STADISCONNECTED:
    os_printf("Wifi AP: station " MACSTR " left, AID = %d\n",
        MAC2STR(evt->event_info.sta_disconnected.mac), evt->event_info.sta_disconnected.aid);
    break;
  default:
    break;
  }

  for (int i = 0; i < 4; i++) {
    if (wifi_state_change_cb[i] != NULL) (wifi_state_change_cb[i])(wifiState);
  }
}

// ===== timers to change state and rescue from failed associations

// reset timer changes back to STA+AP if we can't associate
#define RESET_TIMEOUT (15000) // 15 seconds
static ETSTimer resetTimer;

// This routine is ran some time after a connection attempt to an access point. If
// the connect succeeds, this gets the module in STA-only mode. If it fails, it ensures
// that the module is in STA+AP mode so the user has a chance to recover.
static void ICACHE_FLASH_ATTR resetTimerCb(void *arg) {
  int x = wifi_station_get_connect_status();
  int m = wifi_get_opmode() & 0x3;
  os_printf("Wifi check: mode=%s status=%d\n", wifiMode[m], x);

  if (x == STATION_GOT_IP) {
    if (m != 1) {
      // We're happily connected, go to STA mode
      os_printf("Wifi got IP. Going into STA mode..\n");
      wifi_set_opmode(1);
      wifi_set_sleep_type(SLEEP_MODE);
      os_timer_arm(&resetTimer, RESET_TIMEOUT, 0);
    }
    // no more resetTimer at this point, gotta use physical reset to recover if in trouble
  } else {
    if (m != 3) {
      os_printf("Wifi connect failed. Going into STA+AP mode..\n");
      wifi_set_opmode(3);
    }
    os_printf("Enabling/continuing uart log\n");
    os_timer_arm(&resetTimer, RESET_TIMEOUT, 0);
  }
}

// configure Wifi, specifically DHCP vs static IP address based on flash config
static void ICACHE_FLASH_ATTR configWifiIP() {
    wifi_station_set_hostname("flashConfig.hostname");
    if (wifi_station_dhcpc_status() == DHCP_STARTED)
      wifi_station_dhcpc_stop();
    wifi_station_dhcpc_start();
    os_printf("Wifi uses DHCP, hostname=%s\n", "flashConfig.hostname");
    
  debugIP();
}

// Init the wireless, which consists of setting a timer if we expect to connect to an AP
// so we can revert to STA+AP mode if we can't connect.
void ICACHE_FLASH_ATTR wifiInit() {
  wifi_set_phy_mode(2);
  int x = wifi_get_opmode() & 0x3;
  os_printf("Wifi init, mode=%s\n", wifiMode[x]);
  configWifiIP();

  wifi_set_event_handler_cb(wifiHandleEventCb);
  // check on the wifi in a few seconds to see whether we need to switch mode
  os_timer_disarm(&resetTimer);
  os_timer_setfn(&resetTimer, resetTimerCb, NULL);
  os_timer_arm(&resetTimer, RESET_TIMEOUT, 0);
}

