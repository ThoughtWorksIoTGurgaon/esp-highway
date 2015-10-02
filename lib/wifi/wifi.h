#ifndef WIFI_H
#define WIFI_H

#include "httpd.h"

enum { wifiIsDisconnected, wifiIsConnected, wifiGotIP };
typedef void(*WifiStateChangeCb)(uint8_t wifiStatus);

//WiFi access point data
typedef struct {
  char ssid[32];
  sint8 rssi;
  char enc;
} ApData;

void wifiInit(void);
void startWifiScan(void);
int isWifiScanInProgress(void);
int getNumberOfAPScanned(void);
ApData** getScannedAP(void);
void resetCheck(void);
void setWifiMode(char*); 
void connectWifi(char*, char *);

extern uint8_t wifiState;
extern uint8_t wifiReason;

#endif
