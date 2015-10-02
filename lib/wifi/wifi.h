#ifndef WIFI_H
#define WIFI_H

typedef enum { 
	WifiIsDisconnected, 
	WifiIsConnected, 
	WifiGotIP 
} Wifi_Status;

//WiFi access point data
typedef struct {
  char ssid[32];
  sint8 rssi;
  char enc;
} Wifi_ApData;

typedef void(*Wifi_StateChangeCb)(Wifi_Status);

void Wifi_init(void);
void Wifi_startScan(void);
int Wifi_isScanInProgress(void);
int Wifi_getNumberOfAPScanned(void);
Wifi_ApData** Wifi_getScannedAP(void);
void Wifi_resetCheck(void);
void Wifi_setWifiMode(char*); 
void Wifi_connect(char*, char*);
Wifi_Status Wifi_getStatus();
uint8_t Wifi_getDisconnectionReason();
void Wifi_addStateChangeCb(Wifi_StateChangeCb);

#endif
