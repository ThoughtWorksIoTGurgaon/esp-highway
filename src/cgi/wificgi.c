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

#define CGIWIFI_DBG 

static int ICACHE_FLASH_ATTR _cgiWifiStartScan(HttpdConnData *connData) {
  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.
  
  jsonHeader(connData, 200);
  Wifi_startScan();
  
  return HTTPD_CGI_DONE;
}

static int ICACHE_FLASH_ATTR _cgiWifiGetScan(HttpdConnData *connData) {
  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.
  char buff[2048];
  int len;

  jsonHeader(connData, 200);

  if (Wifi_isScanInProgress()) {
    //We're still scanning. Tell Javascript code that.
    len = os_sprintf(buff, "{\n \"result\": { \n\"inProgress\": \"1\"\n }\n}\n");
    httpdSend(connData, buff, len);
    return HTTPD_CGI_DONE;
  }

  Wifi_ApData **apData = Wifi_getScannedAP();
  int numberOfAPScanned = Wifi_getNumberOfAPScanned();

  len = os_sprintf(buff, "{\"result\": {\"inProgress\": \"0\", \"APs\": [\n");
  for (int pos=0; pos < numberOfAPScanned; pos++) {
    len += os_sprintf(buff+len, "{\"essid\": \"%s\", \"rssi\": %d, \"enc\": \"%d\"}%s\n",
        apData[pos]->ssid, apData[pos]->rssi,
        apData[pos]->enc, (pos==numberOfAPScanned-1)?"":",");
  }
  len += os_sprintf(buff+len, "]}}\n");
  
  //os_printf("Sending %d bytes: %s\n", len, buff);
  httpdSend(connData, buff, len);
  return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR CGI_Wifi_scan(HttpdConnData *connData) {
  if (connData->requestType == HTTPD_METHOD_GET) {
    return _cgiWifiGetScan(connData);
  } else if (connData->requestType == HTTPD_METHOD_POST) {
    return _cgiWifiStartScan(connData);
  } else {
    jsonHeader(connData, 404);
    return HTTPD_CGI_DONE;
  }
}

// This cgi uses the routines above to connect to a specific access point with the
// given ESSID using the given password.
int ICACHE_FLASH_ATTR CGI_Wifi_connect(HttpdConnData *connData) {
  char essid[128];
  char passwd[128];

  if (connData->conn==NULL) return HTTPD_CGI_DONE;

  int el = httpdFindArg(connData->getArgs, "essid", essid, sizeof(essid));
  int pl = httpdFindArg(connData->getArgs, "passwd", passwd, sizeof(passwd));

  if (el > 0 && pl >= 0) {
    Wifi_connect(essid, passwd);
    jsonHeader(connData, 200);
  } else {
    jsonHeader(connData, 400);
    httpdSend(connData, "Cannot parse ssid or password", -1);
  }
  return HTTPD_CGI_DONE;
}

//This cgi changes the operating mode: STA / AP / STA+AP
int ICACHE_FLASH_ATTR CGI_Wifi_setMode(HttpdConnData *connData) {
  int len;
  char buff[1024];

  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.

  len=httpdFindArg(connData->getArgs, "mode", buff, sizeof(buff));
  if (len!=0) {
    Wifi_setWifiMode(buff);
    jsonHeader(connData, 200);
  } else {
    jsonHeader(connData, 400);
  }
  return HTTPD_CGI_DONE;
}

#ifdef CHANGE_TO_STA
#define MODECHANGE "yes"
#else
#define MODECHANGE "no"
#endif

static char *_connStatuses[] = { 
  "idle", 
  "connecting", 
  "wrong password", 
  "AP not found",
  "failed", 
  "got IP address"
};

static char *_wifiMode[] = { 0, "STA", "AP", "AP+STA" };
static char *_wifiPhy[]  = { 0, "11b", "11g", "11n" };

// print various Wifi information into json buffer
static int ICACHE_FLASH_ATTR _printWifiInfo(char *buff) {
  int len;

  struct station_config stconf;
  wifi_station_get_config(&stconf);

  uint8_t op = wifi_get_opmode() & 0x3;
  char *mode = _wifiMode[op];

  char *status = "unknown";
  int st = wifi_station_get_connect_status();
  if (st >= 0 && st < sizeof(_connStatuses)) status = _connStatuses[st];

  int p = wifi_get_phy_mode();
  char *phy = _wifiPhy[p&3];

  sint8 rssi = wifi_station_get_rssi();
  if (rssi > 0) rssi = 0;

  uint8 mac_addr[6];
  wifi_get_macaddr(0, mac_addr);

  uint8_t chan = wifi_get_channel();

  len = os_sprintf(buff,
    "\"mode\": \"%s\", \"modechange\": \"%s\", \"ssid\": \"%s\", \"status\": \"%s\", \"phy\": \"%s\", "
    "\"rssi\": \"%ddB\", \"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\", \"chan\":%d",
    mode, MODECHANGE, (char*)stconf.ssid, status, phy, rssi,
    mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5], chan);

  struct ip_info info;
  if (wifi_get_ip_info(0, &info)) {
    len += os_sprintf(buff+len, ", \"ip\": \"%d.%d.%d.%d\"", IP2STR(&info.ip.addr));
    len += os_sprintf(buff+len, ", \"netmask\": \"%d.%d.%d.%d\"", IP2STR(&info.netmask.addr));
    len += os_sprintf(buff+len, ", \"gateway\": \"%d.%d.%d.%d\"", IP2STR(&info.gw.addr));
    len += os_sprintf(buff+len, ", \"hostname\": \"%s\"", "my-esp");
  } else {
    len += os_sprintf(buff+len, ", \"ip\": \"-none-\"");
  }
  // len += os_sprintf(buff+len, ", \"staticip\": \"%d.%d.%d.%d\"", IP2STR(&flashConfig.staticip));
  // len += os_sprintf(buff+len, ", \"dhcp\": \"%s\"", flashConfig.staticip > 0 ? "off" : "on");

  return len;
}

// reasons for which a connection failed
static char *_wifiReasons[] = {
  "", "unspecified", "auth_expire", "auth_leave", "assoc_expire", "assoc_toomany", "not_authed",
  "not_assoced", "assoc_leave", "assoc_not_authed", "disassoc_pwrcap_bad", "disassoc_supchan_bad",
  "ie_invalid", "mic_failure", "4way_handshake_timeout", "group_key_update_timeout",
  "ie_in_4way_differs", "group_cipher_invalid", "pairwise_cipher_invalid", "akmp_invalid",
  "unsupp_rsn_ie_version", "invalid_rsn_ie_cap", "802_1x_auth_failed", "cipher_suite_rejected",
  "beacon_timeout", "no_ap_found" };

static char* ICACHE_FLASH_ATTR _wifiGetReason() {
  uint8 wifiReason = Wifi_getDisconnectionReason();
  if (wifiReason <= 24) return _wifiReasons[wifiReason];
  if (wifiReason >= 200 && wifiReason <= 201) return _wifiReasons[wifiReason-200+24];
  return _wifiReasons[1];
}

// Cgi to return various Wifi information
int ICACHE_FLASH_ATTR CGI_Wifi_info(HttpdConnData *connData) {
  char buff[1024];

  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.

  os_strcpy(buff, "{");
  _printWifiInfo(buff+1);
    len += os_sprintf(buff+len, ", ");

  if (Wifi_getDisconnectionReason() != 0) {
    len += os_sprintf(buff+len, "\"reason\": \"%s\"", _wifiGetReason());
  }

  os_strcat(buff, " }");

  jsonHeader(connData, 200);
  httpdSend(connData, buff, -1);
  return HTTPD_CGI_DONE;
}
