/*
Cgi/template routines for the /wifi url.
*/

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

#include "cgi.h"
#include "wifi.h"

#define WIFI_DBG 

//#define SLEEP_MODE LIGHT_SLEEP_T
#define SLEEP_MODE MODEM_SLEEP_T

// ===== wifi status change callbacks
static WifiStateChangeCb wifi_state_change_cb[4];

uint8_t wifiState = wifiIsDisconnected;

uint8_t wifiReason = 0;

static char *wifiMode[] = { 0, "STA", "AP", "AP+STA" };

void (*wifiStatusCb)(uint8_t); // callback when wifi status changes

// handler for wifi status change callback coming in from espressif library
static void ICACHE_FLASH_ATTR wifiHandleEventCb(System_Event_t *evt) {
  switch (evt->event) {
  case EVENT_STAMODE_CONNECTED:
    wifiState = wifiIsConnected;
    wifiReason = 0;
#ifdef WIFI_DBG
    os_printf("Wifi connected to ssid %s, ch %d\n", evt->event_info.connected.ssid,
        evt->event_info.connected.channel);
#endif
    break;
  case EVENT_STAMODE_DISCONNECTED:
    wifiState = wifiIsDisconnected;
    wifiReason = evt->event_info.disconnected.reason;
#ifdef WIFI_DBG
    os_printf("Wifi disconnected from ssid %s, reason %d (%d)\n",
        evt->event_info.disconnected.ssid, wifiReason, evt->event_info.disconnected.reason);
#endif
    break;
  case EVENT_STAMODE_AUTHMODE_CHANGE:
#ifdef WIFI_DBG
    os_printf("Wifi auth mode: %d -> %d\n",
        evt->event_info.auth_change.old_mode, evt->event_info.auth_change.new_mode);
#endif
    break;
  case EVENT_STAMODE_GOT_IP:
    wifiState = wifiGotIP;
    wifiReason = 0;
#ifdef WIFI_DBG
    os_printf("Wifi got ip:" IPSTR ",mask:" IPSTR ",gw:" IPSTR "\n",
        IP2STR(&evt->event_info.got_ip.ip), IP2STR(&evt->event_info.got_ip.mask),
        IP2STR(&evt->event_info.got_ip.gw));
#endif
    break;
  case EVENT_SOFTAPMODE_STACONNECTED:
#ifdef WIFI_DBG
    os_printf("Wifi AP: station " MACSTR " joined, AID = %d\n",
        MAC2STR(evt->event_info.sta_connected.mac), evt->event_info.sta_connected.aid);
#endif
    break;
  case EVENT_SOFTAPMODE_STADISCONNECTED:
#ifdef WIFI_DBG
    os_printf("Wifi AP: station " MACSTR " left, AID = %d\n",
        MAC2STR(evt->event_info.sta_disconnected.mac), evt->event_info.sta_disconnected.aid);
#endif
    break;
  default:
    break;
  }

  for (int i = 0; i < 4; i++) {
    if (wifi_state_change_cb[i] != NULL) (wifi_state_change_cb[i])(wifiState);
  }
}

// ===== wifi scanning

//Scan result
typedef struct {
  char scanInProgress; //if 1, don't access the underlying stuff from the webpage.
  ApData **apData;
  int noAps;
} ScanResultData;

//Static scan status storage.
static ScanResultData cgiWifiAps;

//Callback the code calls when a wlan ap scan is done. Basically stores the result in
//the cgiWifiAps struct.
void ICACHE_FLASH_ATTR wifiScanDoneCb(void *arg, STATUS status) {
  int n;
  struct bss_info *bss_link = (struct bss_info *)arg;

  if (status!=OK) {
#ifdef WIFI_DBG
    os_printf("wifiScanDoneCb status=%d\n", status);
#endif
    cgiWifiAps.scanInProgress=0;
    return;
  }

  //Clear prev ap data if needed.
  if (cgiWifiAps.apData!=NULL) {
    for (n=0; n<cgiWifiAps.noAps; n++) os_free(cgiWifiAps.apData[n]);
    os_free(cgiWifiAps.apData);
  }

  //Count amount of access points found.
  n=0;
  while (bss_link != NULL) {
    bss_link = bss_link->next.stqe_next;
    n++;
  }
  //Allocate memory for access point data
  cgiWifiAps.apData=(ApData **)os_malloc(sizeof(ApData *)*n);
  cgiWifiAps.noAps=n;
#ifdef WIFI_DBG
  os_printf("Scan done: found %d APs\n", n);
#endif

  //Copy access point data to the static struct
  n=0;
  bss_link = (struct bss_info *)arg;
  while (bss_link != NULL) {
    if (n>=cgiWifiAps.noAps) {
      //This means the bss_link changed under our nose. Shouldn't happen!
      //Break because otherwise we will write in unallocated memory.
#ifdef WIFI_DBG
      os_printf("Huh? I have more than the allocated %d aps!\n", cgiWifiAps.noAps);
#endif
      break;
    }
    //Save the ap data.
    cgiWifiAps.apData[n]=(ApData *)os_malloc(sizeof(ApData));
    cgiWifiAps.apData[n]->rssi=bss_link->rssi;
    cgiWifiAps.apData[n]->enc=bss_link->authmode;
    strncpy(cgiWifiAps.apData[n]->ssid, (char*)bss_link->ssid, 32);
#ifdef WIFI_DBG
    os_printf("bss%d: %s (%d)\n", n+1, (char*)bss_link->ssid, bss_link->rssi);
#endif

    bss_link = bss_link->next.stqe_next;
    n++;
  }
  //We're done.
  cgiWifiAps.scanInProgress=0;
}

static ETSTimer scanTimer;
static void ICACHE_FLASH_ATTR scanStartCb(void *arg) {
#ifdef WIFI_DBG
  os_printf("Starting a scan\n");
#endif
  wifi_station_scan(NULL, wifiScanDoneCb);
}

void ICACHE_FLASH_ATTR startWifiScan() {
  if (!cgiWifiAps.scanInProgress) {
    cgiWifiAps.scanInProgress = 1;
    os_timer_disarm(&scanTimer);
    os_timer_setfn(&scanTimer, scanStartCb, NULL);
    os_timer_arm(&scanTimer, 1000, 0);
  }
}

int ICACHE_FLASH_ATTR isWifiScanInProgress() {
  return cgiWifiAps.scanInProgress;
}

int ICACHE_FLASH_ATTR getNumberOfAPScanned() {
  return cgiWifiAps.noAps;
}

ApData** ICACHE_FLASH_ATTR getScannedAP() {
  return cgiWifiAps.apData;
}


// Temp store for new ap info.
static struct station_config stconf;
// Reassociate timer to delay change of association so the original request can finish
static ETSTimer reassTimer;

// Callback actually doing reassociation
static void ICACHE_FLASH_ATTR reassTimerCb(void *arg) {
#ifdef CGIWIFI_DBG
  os_printf("Wifi changing association\n");
#endif
  wifi_station_disconnect();
  stconf.bssid_set = 0;
  wifi_station_set_config(&stconf);
  wifi_station_connect();
  
  // Schedule check
  resetCheck();
}

void ICACHE_FLASH_ATTR connectWifi(char *essid, char *passwd) {
    //Set to 0 if you want to disable the actual reconnecting bit
    os_strncpy((char*)stconf.ssid, essid, 32);
    os_strncpy((char*)stconf.password, passwd, 64);

#ifdef WIFI_DBG
    os_printf("Wifi try to connect to AP %s pw %s\n", essid, passwd);
#endif

    //Schedule disconnect/connect
    os_timer_disarm(&reassTimer);
    os_timer_setfn(&reassTimer, reassTimerCb, NULL);
    os_timer_arm(&reassTimer, 1000, 0);
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
#ifdef WIFI_DBG
  os_printf("Wifi check: mode=%s status=%d\n", wifiMode[m], x);
#endif

  if (x == STATION_GOT_IP) {
    if (m != 1) {
#ifdef CHANGE_TO_STA
      // We're happily connected, go to STA mode
#ifdef WIFI_DBG
      os_printf("Wifi got IP. Going into STA mode..\n");
#endif
      wifi_set_opmode(1);
      wifi_set_sleep_type(SLEEP_MODE);
      os_timer_arm(&resetTimer, RESET_TIMEOUT, 0);
#endif
    }
    // no more resetTimer at this point, gotta use physical reset to recover if in trouble
  } else {
    if (m != 3) {
#ifdef WIFI_DBG
      os_printf("Wifi connect failed. Going into STA+AP mode..\n");
#endif
      wifi_set_opmode(3);
    }
#ifdef WIFI_DBG
    os_printf("Enabling/continuing uart log\n");
#endif
    os_timer_arm(&resetTimer, RESET_TIMEOUT, 0);
  }
}

// check on the wifi in a few seconds to see whether we need to switch mode
void ICACHE_FLASH_ATTR resetCheck() {
  os_timer_disarm(&resetTimer);
  os_timer_setfn(&resetTimer, resetTimerCb, NULL);
  os_timer_arm(&resetTimer, RESET_TIMEOUT, 0);
}

void ICACHE_FLASH_ATTR setWifiMode(char *mode) {
  int m = atoi(mode);
  #ifdef WIFI_DBG
      os_printf("Wifi switching to mode %d\n", m);
  #endif
  wifi_set_opmode(m&3);
  if (m == 1) {
    wifi_set_sleep_type(SLEEP_MODE);
    // STA-only mode, reset into STA+AP after a timeout
    os_timer_disarm(&resetTimer);
    os_timer_setfn(&resetTimer, resetTimerCb, NULL);
    os_timer_arm(&resetTimer, RESET_TIMEOUT, 0);
  }
}

#ifdef DEBUGIP
static void ICACHE_FLASH_ATTR debugIP() {
  struct ip_info info;
  if (wifi_get_ip_info(0, &info)) {
    os_printf("\"ip\": \"%d.%d.%d.%d\"\n", IP2STR(&info.ip.addr));
    os_printf("\"netmask\": \"%d.%d.%d.%d\"\n", IP2STR(&info.netmask.addr));
    os_printf("\"gateway\": \"%d.%d.%d.%d\"\n", IP2STR(&info.gw.addr));
    os_printf("\"hostname\": \"%s\"\n", wifi_station_get_hostname());
  } else {
    os_printf("\"ip\": \"-none-\"\n");
  }
}
#endif

// Init the wireless, which consists of setting a timer if we expect to connect to an AP
// so we can revert to STA+AP mode if we can't connect.
void ICACHE_FLASH_ATTR wifiInit() {
  wifi_set_phy_mode(2);
#ifdef WIFI_DBG
  int x = wifi_get_opmode() & 0x3;
  os_printf("Wifi init, mode=%s\n", wifiMode[x]);
#endif

#ifdef DEBUGIP
  debugIP();
#endif

  wifi_set_event_handler_cb(wifiHandleEventCb);
  
  resetCheck();
}