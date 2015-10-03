#ifdef MQTT
#ifndef CGIMQTT_H
#define CGIMQTT_H

#include "httpd.h"

int CGI_Mqtt_setConfig(HttpdConnData *connData);
int CGI_Mqtt_getConfig(HttpdConnData *connData);

#endif // CGIMQTT_H
#endif // MQTT
