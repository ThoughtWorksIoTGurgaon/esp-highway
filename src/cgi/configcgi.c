#include <esp8266.h>
#include "cgi.h"
#include "config.h"

#define CGICONFIG_DBG

int ICACHE_FLASH_ATTR CGI_Config_saveConfig(HttpdConnData *connData) {
#ifdef CGICONFIG_DBG
  os_printf("Saving config\n");
#endif
  if (configSave()) {
    httpdStartResponse(connData, 200);
    httpdEndHeaders(connData);
  } else {
    httpdStartResponse(connData, 500);
    httpdEndHeaders(connData);
    httpdSend(connData, "Failed to save config", -1);
  }
  return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR CGI_Config_wipeConfig(HttpdConnData *connData) {
#ifdef CGICONFIG_DBG
  os_printf("Wiping out config\n");
#endif
  configWipe();
  httpdStartResponse(connData, 200);
  httpdEndHeaders(connData);

  return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR CGI_Config_restoreConfig(HttpdConnData *connData) {
#ifdef CGICONFIG_DBG
  os_printf("Restoring out config\n");
#endif
  if (configRestore()) {
    httpdStartResponse(connData, 200);
    httpdEndHeaders(connData);
  } else {
    httpdStartResponse(connData, 500);
    httpdEndHeaders(connData);
    httpdSend(connData, "Failed to restore config", -1);
  }

  return HTTPD_CGI_DONE;
}