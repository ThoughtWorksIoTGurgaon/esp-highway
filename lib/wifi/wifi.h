#ifndef CGIWIFI_H
#define CGIWIFI_H

enum { wifiIsDisconnected, wifiIsConnected, wifiGotIP };
typedef void(*WifiStateChangeCb)(uint8_t wifiStatus);

void wifiInit(void);

extern uint8_t wifiState;

#endif