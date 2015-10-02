#include <esp8266.h>

#include "lib/uart/uart.h"
#include "lib/wifi/wifi.h"
#include "lib/httpd/httpd.h"

#define SHOW_HEAP_USE

HttpdBuiltInUrl builtInUrls[] = {
  { "/wifi/info", cgiWifiInfo, NULL },
  { "/wifi/scan", cgiWiFiScan, NULL },
  { "/wifi/connect", cgiWiFiConnect, NULL },
  { "/wifi/connstatus", cgiWiFiConnStatus, NULL },
  { "/wifi/setmode", cgiWiFiSetMode, NULL },
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

    wifiInit();

    httpdInit(builtInUrls, 80);

#ifdef SHOW_HEAP_USE
  os_timer_disarm(&prHeapTimer);
  os_timer_setfn(&prHeapTimer, prHeapTimerCb, NULL);
  os_timer_arm(&prHeapTimer, 2000, 1);
#endif  
}