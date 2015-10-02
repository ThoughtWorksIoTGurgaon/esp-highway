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

// ===== wifi scanning

//WiFi access point data
typedef struct {
  char ssid[32];
  sint8 rssi;
  char enc;
} ApData;

//Scan result
typedef struct {
  char scanInProgress; //if 1, don't access the underlying stuff from the webpage.
  ApData **apData;
  int noAps;
} ScanResultData;

//Static scan status storage.
static ScanResultData wifiAps;

//Callback the code calls when a wlan ap scan is done. Basically stores the result in
//the wifiAps struct.
void ICACHE_FLASH_ATTR wifiScanDoneCb(void *arg, STATUS status) {
  int n;
  struct bss_info *bss_link = (struct bss_info *)arg;

  if (status!=OK) {
    os_printf("wifiScanDoneCb status=%d\n", status);
    wifiAps.scanInProgress=0;
    return;
  }

  //Clear prev ap data if needed.
  if (wifiAps.apData!=NULL) {
    for (n=0; n<wifiAps.noAps; n++) os_free(wifiAps.apData[n]);
    os_free(wifiAps.apData);
  }

  //Count amount of access points found.
  n=0;
  while (bss_link != NULL) {
    bss_link = bss_link->next.stqe_next;
    n++;
  }
  //Allocate memory for access point data
  wifiAps.apData=(ApData **)os_malloc(sizeof(ApData *)*n);
  wifiAps.noAps=n;
  os_printf("Scan done: found %d APs\n", n);

  //Copy access point data to the static struct
  n=0;
  bss_link = (struct bss_info *)arg;
  while (bss_link != NULL) {
    if (n>=wifiAps.noAps) {
      //This means the bss_link changed under our nose. Shouldn't happen!
      //Break because otherwise we will write in unallocated memory.
      os_printf("Huh? I have more than the allocated %d aps!\n", wifiAps.noAps);
      break;
    }

    //Save the ap data.
    wifiAps.apData[n]=(ApData *)os_malloc(sizeof(ApData));
    wifiAps.apData[n]->rssi=bss_link->rssi;
    wifiAps.apData[n]->enc=bss_link->authmode;
    strncpy(wifiAps.apData[n]->ssid, (char*)bss_link->ssid, 32);
    os_printf("bss%d: %s (%d)\n", n+1, (char*)bss_link->ssid, bss_link->rssi);

    bss_link = bss_link->next.stqe_next;
    n++;
  }

  //We're done.
  wifiAps.scanInProgress = 0;
}

#define AP_LIST_REFRESH (2000) // 15 seconds
static ETSTimer scanTimer;
static void ICACHE_FLASH_ATTR scanStartCb(void *arg) {
  if (!wifiAps.scanInProgress) {
    wifiAps.scanInProgress = 1;
    os_printf("Starting a scan\n");
    wifi_station_scan(NULL, wifiScanDoneCb);
  }
}

void ICACHE_FLASH_ATTR wifiStartScan() {
  wifiAps.scanInProgress = 0;
  os_timer_disarm(&scanTimer);
  os_timer_setfn(&scanTimer, scanStartCb, NULL);
  os_timer_arm(&scanTimer, AP_LIST_REFRESH, 0);
}

int ICACHE_FLASH_ATTR isWifiScanInProgre() {
  return wifiAps.scanInProgress;
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

// Init the wireless, which consists of setting a timer if we expect to connect to an AP
// so we can revert to STA+AP mode if we can't connect.
void ICACHE_FLASH_ATTR wifiInit() {
  wifi_set_phy_mode(2);
  int x = wifi_get_opmode() & 0x3;
  os_printf("Wifi init, mode=%s\n", wifiMode[x]);

  wifi_set_event_handler_cb(wifiHandleEventCb);

  // check on the wifi in a few seconds to see whether we need to switch mode
  os_timer_disarm(&resetTimer);
  os_timer_setfn(&resetTimer, resetTimerCb, NULL);
  os_timer_arm(&resetTimer, RESET_TIMEOUT, 0);
}

