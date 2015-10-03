#include <esp8266.h>

#include "uart.h"
#include "wifi.h"
#include "httpd.h"

#include "config.h"
#include "mqtt_client.h"

#include "wificgi.h"
#include "configcgi.h"
#include "mqttcgi.h"

#define SHOW_HEAP_USE

HttpdBuiltInUrl builtInUrls[] = {
  { "/wifi/info", CGI_Wifi_info, NULL },
  { "/wifi/scan", CGI_Wifi_scan, NULL },
  { "/wifi/connect", CGI_Wifi_connect, NULL },
  { "/wifi/connstatus", CGI_Wifi_connStatus, NULL },
  { "/wifi/setmode", CGI_Wifi_setMode, NULL },

  { "/mqtt/getConf", CGI_Mqtt_getConfig, NULL },
  { "/mqtt/setConf", CGI_Mqtt_setConfig, NULL },

  { "/config/wipe", CGI_Config_wipeConfig, NULL },
  { "/config/restore", CGI_Config_restoreConfig, NULL },
  { "/config/save", CGI_Config_saveConfig, NULL },

  { NULL, NULL, NULL }
};

#ifdef SHOW_HEAP_USE
static ETSTimer prHeapTimer;

static void ICACHE_FLASH_ATTR prHeapTimerCb(void *arg) {
  os_printf("Heap: %ld\n", (unsigned long)system_get_free_heap_size());
}
#endif

static char *rst_codes[] = {
  "normal", "wdt reset", "exception", "soft wdt", "restart", "deep sleep", "external",
};
static char *flash_maps[] = {
  "512KB:256/256", "256KB", "1MB:512/512", "2MB:512/512", "4MB:512/512",
  "2MB:1024/1024", "4MB:1024/1024"
};

void ICACHE_FLASH_ATTR user_init()
{
  // get the flash config so we know how to init things
  // configWipe(); // uncomment to reset the config for testing purposes
  bool restoreOk = configRestore();
    uart_init(115200, 115200);

    os_printf("Starting--\n\r");

    os_delay_us(10000L);
    os_printf("--Starting\n\r");
    os_printf("Flash config restore %s\n", restoreOk ? "ok" : "*FAILED*");
    os_printf("mqqt host : %s\n", flashConfig.mqtt_host);
    os_printf("mqqt client id : %s\n", flashConfig.mqtt_clientid);
    
    Wifi_init();

    httpdInit(builtInUrls, 80);

    mqtt_client_init();


  struct rst_info *rst_info = system_get_rst_info();
  os_printf("Reset cause: %d=%s\n", rst_info->reason, rst_codes[rst_info->reason]);
  os_printf("exccause=%d epc1=0x%x epc2=0x%x epc3=0x%x excvaddr=0x%x depc=0x%x\n",
    rst_info->exccause, rst_info->epc1, rst_info->epc2, rst_info->epc3,
    rst_info->excvaddr, rst_info->depc);
  uint32_t fid = spi_flash_get_id();
  os_printf("Flash map %s, manuf 0x%02lX chip 0x%04lX\n", flash_maps[system_get_flash_size_map()],
      fid & 0xff, (fid&0xff00)|((fid>>16)&0xff));


#ifdef SHOW_HEAP_USE
  os_timer_disarm(&prHeapTimer);
  os_timer_setfn(&prHeapTimer, prHeapTimerCb, NULL);
  os_timer_arm(&prHeapTimer, 2000, 1);
#endif  
}