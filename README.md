ESP-HIGHWAY
========

This firmware connects an attached micro-controller to the internet using a ESP8266 Wifi module.

Mostly taken from esp-link, it implements a number of features:
- configuration of wifi using CURL
- goes into AccessPoint mode if it is not able to connect to configured AP.
- runs a mqtt client
- configuration of mqtt params like host, port, client-id, etc
- [forthcoming] attached uC can communicate with the native mqtt client.

Esp-highway uses
-------------
The simplest use of esp-highway is to serve as mqtt client. 

An additional option is to add code to esp-higway to customize it and put all the communication
code into esp-highway and only keep simple sensor/actuator control in the attached uC. In this
mode the attached uC sends custom commands to esp-higway with sensor/acturator info and
registers a set of callbacks with esp-highway that control sensors/actuators. This way, custom
commands in esp-highway can receive MQTT messages, make simple callbacks into the uC to get sensor
values or change actuators, and then respond back with MQTT. The way this is architected is that
the attached uC registers callbacks at start-up such that the code in the esp doesn't need to 
know which exact sensors/actuators the attached uC has, it learns thta through the initial
callback registration.

Hardware info
-------------
This firmware is designed for esp8266 modules which have most ESP I/O pins available and
at least 1MB flash. (The V1 firmware supports modules with 512KB flash).
The default connections are:
- URXD: connect to TX of microcontroller
- UTXD: connect to RX of microcontroller
- GPIO12: connect to RESET of microcontroller
- GPIO13: connect to ISP of LPC/ARM microcontroller (not used with Arduino/AVR)
- GPIO0: optionally connect green "conn" LED to 3.3V (indicates wifi status)
- GPIO2: optionally connect yellow "ser" LED to 3.3V (indicates serial activity)

If you are using an FTDI connector, GPIO12 goes to DTR and GPIO13 goes to CTS.

If you are using an esp-12 module, you can avoid the initial boot message from the esp8266
bootloader by using the swap-pins option. This swaps the esp8266 TX/RX to gpio15/gpio13 respectively.

The GPIO pin assignments can be changed dynamically in the web UI and are saved in flash.

Initial flashing
----------------
(This is not necessary if you receive one of the jn-esp or esp-bridge modules from the author!)
If you want to simply flash the provided firmware binary and use your favorite
ESP8266 flashing tool to flash the bootloader, the firmware, and blank settings.
Detailed instructions are provided in the release notes.

Note that the firmware assumes a 512KB flash chip, which most of the esp-01 thru esp-11
modules appear to have. A larger flash chip should work but has not been tested.

Wifi configuration overview
------------------
For proper operation the end state the esp-higway needs to arrive at is to have it
join your pre-existing wifi network as a pure station.
However, in order to get there the esp-higway will start out as an access point and you'll have
to join its network to configure it. The short version is:
 1. the esp-higway creates a wifi access point with an SSID of the form `ESP_XXXXX`
 2. you join your laptop or phone to the esp-higway's network as a station and you configure
    the esp-higway wifi with your network info by performing some commands on ip 192.168.4.1
 3. the esp-higway starts to connect to your network while continuing to also be an access point
    ("AP+STA").
 4. the esp-higway succeeds in connecting and shuts down its own access point after 15 seconds,
    you reconnect your laptop/phone to your normal network and access esp-higway via its IP address

Wifi configuration details
--------------------------
After you have serially flashed the module it will create a wifi access point (AP) with an
SSID of the form `ESP_012ABC` where 012ABC is a piece of the module's MAC address.

Connect to this SSID and open up a terminal on your laptop. Now you can perform the following commands.
1. curl '192.168.4.1/wifi/info' #to get the wifi config details
2. curl '192.168.4.1/wifi/scan' #to start a scan for available Access Points
3. curl '192.168.4.1/wifi/scan' -X POST #to get the list of scanned APs
4. curl '192.168.4.1/wifi/connect?essid=SSID&passwd=PASSWORD' #to connect to AP with ssid=SSID
5. curl '192.168.4.1/wifi/setMode?mode=MODE' #to set the mode of AP, MODE=(STA|AP|AP+STA)

There is a fail-safe, which is that after a reset or a configuration change, if the esp-highway
cannot connect to your network it will revert back to AP+STA mode after 15 seconds and thus
both present its `ESP_012ABC`-style network and continue trying to reconnect to the requested network.
You can then connect to the esp-highway's AP and reconfigure the station part.

One open issue (#28) is that esp-highway cannot always display the IP address it is getting to the browser
used to configure the ssid/password info. The problem is that the initial STA+AP mode may use
channel 1 and you configure it to connect to an AP on channel 6. This requires the ESP8266's AP
to also switch to channel 6 disconnecting you in the meantime. 

MQTT configuration
------------------

Use following commands to configure mqtt params.
1. curl '192.168.4.1/mqtt/getConf' #to get the configuration details
2. curl '192.168.4.1/mqtt/setConf?mqtt-enable=true&mqtt-host=IP&mqtt-client-id=CLIENT_ID&mqtt-port=PORT' #to set the configuration


Saving configuration
--------------------

To save the mqtt configurations use follwoing commands
1. curl '192.168.4.1/config/wipe' #to wipe out saved configuration, to revert to defaults do restore after wipe or perform reset
2. curl '192.168.4.1/config/save' #to save the configuration, this will retain config in case of resets
3. curl '192.168.4.1/config/restore' #to manually restore configuration, this will discard any configration changes done after a config save

Troubleshooting
---------------
- verify that you have sufficient power, borderline power can cause the esp module to seemingly
  function until it tries to transmit and the power rail collapses
- reset or power-cycle the esp-higway to force it to become an access-point if it can't
  connect to your network within 15-20 seconds
- if you do not know the esp-highway's IP address on your network, turn off your access point (or walk
  far enough away) and reset/power-cycle esp-highway, it will then fail to connect and start its
  own AP after 15-20 seconds

Building the firmware
---------------------
The firmware has been built using the [esp-open-sdk](https://github.com/pfalcon/esp-open-sdk)
on a Linux system. 

Follow the following steps to build the firmware.
1. 

If you choose a different directory structure look at the Makefile for the appropriate environment
variables to define.

Now, build the code: `make` in the top-level of esp-highway.

A few notes from others (I can't fully verify these):
- You may need to install `zlib1g-dev` and `python-serial`
- Make sure you have the correct version of the esp_iot_sdk
- Make sure the paths at the beginning of the makefile are correct
- Make sure `esp-open-sdk/xtensa-lx106-elf/bin` is in the PATH set in the Makefile

SLIP & MQTT
---------------------------
Esp-higway support the espduino SLIP protocol that supports simple outbound
MQTT requests. The SLIP protocol consists of commands with binary arguments sent from the
attached microcontroller to the esp8266, which then performs the command and responds back.
The responses back use a callback address in the attached microcontroller code, i.e., the
command sent by the uC contains a callback address and the response from the esp8266 starts
with that callback address. This enables asynchronous communication where esp-highway can notify the
uC when requests complete or when other actions happen, such as wifi connectivity status changes. Using slip
mqtt callbacks can be attached to the native mqtt client running on esp8266.

Contact
-------
If you find problems with esp-highway, please create a github issue.
