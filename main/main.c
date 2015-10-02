#include <esp8266.h>

#include "lib/uart/uart.h"
#include "lib/wifi/wifi.h"
    
//Init function 
void ICACHE_FLASH_ATTR
user_init()
{
    uart_init(115200, 115200);

    os_printf("Starting\n\r");

    wifiInit();
    wifiStartScan();
}