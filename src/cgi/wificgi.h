#ifndef CGIWIFI_H
#define CGIWIFI_H

#include "httpd.h"

int CGI_Wifi_scan(HttpdConnData *connData);
int CGI_Wifi_info(HttpdConnData *connData);
int CGI_Wifi_connect(HttpdConnData *connData);
int CGI_Wifi_setMode(HttpdConnData *connData);

#endif
