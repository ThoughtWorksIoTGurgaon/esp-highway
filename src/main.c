#include <esp8266.h>

#include "uart.h"
#include "wifi.h"
#include "httpd.h"

#include "wificgi.h"

// #define SHOW_HEAP_USE

HttpdBuiltInUrl builtInUrls[] = {
  { "/wifi/info", CGI_Wifi_info, NULL },
  { "/wifi/scan", CGI_Wifi_scan, NULL },
  { "/wifi/connect", CGI_Wifi_connect, NULL },
  { "/wifi/connstatus", CGI_Wifi_connStatus, NULL },
  { "/wifi/setmode", CGI_Wifi_setMode, NULL },
  { NULL, NULL, NULL }
};


#ifdef SHOW_HEAP_USE
static ETSTimer prHeapTimer;

static void ICACHE_FLASH_ATTR prHeapTimerCb(void *arg) {
  os_printf("Heap: %ld\n", (unsigned long)system_get_free_heap_size());
}
#endif

void ICACHE_FLASH_ATTR user_init()
{
    uart_init(115200, 115200);

    os_printf("Starting\n\r");

    Wifi_init();

    httpdInit(builtInUrls, 80);

#ifdef SHOW_HEAP_USE
  os_timer_disarm(&prHeapTimer);
  os_timer_setfn(&prHeapTimer, prHeapTimerCb, NULL);
  os_timer_arm(&prHeapTimer, 2000, 1);
#endif  
}