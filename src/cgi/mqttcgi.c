// Copyright 2015 by Thorsten von Eicken, see LICENSE.txt
#ifdef MQTT
#include <esp8266.h>
#include "cgi.h"
#include "config.h"
#include "mqtt_client.h"
#include "mqttcgi.h"

#define CGIMQTT_DBG

static char *mqtt_states[] = {
  "disconnected", "reconnecting", "connecting", "connected",
};

// Cgi to return MQTT settings
int ICACHE_FLASH_ATTR CGI_Mqtt_getConfig(HttpdConnData *connData) {
  char buff[1024];
  int len;

  if (connData->conn==NULL) return HTTPD_CGI_DONE;

  len = os_sprintf(buff, "{ "
      "\"mqtt-enable\":%d, "
      "\"mqtt-state\":\"%s\", "
      "\"mqtt-port\":%d, "
      "\"mqtt-timeout\":%d, "
      "\"mqtt-keepalive\":%d, "
      "\"mqtt-host\":\"%s\", "
      "\"mqtt-client-id\":\"%s\", "
      "\"mqtt-username\":\"%s\", "
      "\"mqtt-password\":\"%s\""
      "  }",
      flashConfig.mqtt_enable,
      mqtt_states[mqttClient.connState], 
      flashConfig.mqtt_port,
      flashConfig.mqtt_timeout,
      flashConfig.mqtt_keepalive,
      flashConfig.mqtt_host,
      flashConfig.mqtt_clientid,
      flashConfig.mqtt_username,
      flashConfig.mqtt_password
    );

  jsonHeader(connData, 200);
  httpdSend(connData, buff, len);
  return HTTPD_CGI_DONE;
}

// Cgi to change choice of pin assignments
int ICACHE_FLASH_ATTR CGI_Mqtt_setConfig(HttpdConnData *connData) {
  if (connData->conn==NULL) return HTTPD_CGI_DONE;

  int8_t mqtt_en_chg = getBoolArg(connData, "mqtt-enable",
      &flashConfig.mqtt_enable);

  // handle MQTT server settings
  int8_t mqtt_server = 0; // accumulator for changes/errors
  mqtt_server |= getStringArg(connData, "mqtt-host",
      flashConfig.mqtt_host, sizeof(flashConfig.mqtt_host));
  if (mqtt_server < 0) return HTTPD_CGI_DONE;

  mqtt_server |= getStringArg(connData, "mqtt-client-id",
      flashConfig.mqtt_clientid, sizeof(flashConfig.mqtt_clientid));
  if (mqtt_server < 0) return HTTPD_CGI_DONE;

  mqtt_server |= getStringArg(connData, "mqtt-username",
      flashConfig.mqtt_username, sizeof(flashConfig.mqtt_username));
  if (mqtt_server < 0) return HTTPD_CGI_DONE;

  mqtt_server |= getStringArg(connData, "mqtt-password",
      flashConfig.mqtt_password, sizeof(flashConfig.mqtt_password));
  if (mqtt_server < 0) return HTTPD_CGI_DONE;

  char buff[32];
  // handle mqtt port
  if (httpdFindArg(connData->getArgs, "mqtt-port", buff, sizeof(buff)) > 0) {
    int32_t port = atoi(buff);
    if (port > 0 && port < 65536) {
      flashConfig.mqtt_port = port;
      mqtt_server |= 1;
    } else {
      errorResponse(connData, 400, "Invalid MQTT port");
      return HTTPD_CGI_DONE;
    }
  }

  // handle mqtt timeout
  if (httpdFindArg(connData->getArgs, "mqtt-timeout", buff, sizeof(buff)) > 0) {
    int32_t timeout = atoi(buff);
    flashConfig.mqtt_timeout = timeout;
  }

  // handle mqtt keepalive
  if (httpdFindArg(connData->getArgs, "mqtt-keepalive", buff, sizeof(buff)) > 0) {
    int32_t keepalive = atoi(buff);
    flashConfig.mqtt_keepalive = keepalive;
  }

  // if server setting changed, we need to "make it so"
  if (mqtt_server) {
#ifdef CGIMQTT_DBG
    os_printf("MQTT server settings changed, enable=%d\n", flashConfig.mqtt_enable);
#endif
    MQTT_Free(&mqttClient); // safe even if not connected
    mqtt_client_init();

  // if just enable changed we just need to bounce the client
  } else if (mqtt_en_chg > 0) {
#ifdef CGIMQTT_DBG
    os_printf("MQTT server enable=%d changed\n", flashConfig.mqtt_enable);
#endif
    if (flashConfig.mqtt_enable && strlen(flashConfig.mqtt_host) > 0)
      MQTT_Reconnect(&mqttClient);
    else
      MQTT_Disconnect(&mqttClient);
  }

  return CGI_Mqtt_getConfig(connData);
}

#endif // MQTT
