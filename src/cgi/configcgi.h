#ifndef CGICONFIG_H
#define CGICONFIG_H

#include "httpd.h"

int CGI_Config_saveConfig(HttpdConnData *connData);
int CGI_Config_wipeConfig(HttpdConnData *connData);
int CGI_Config_restoreConfig(HttpdConnData *connData);

#endif // CGICONFIG_H
