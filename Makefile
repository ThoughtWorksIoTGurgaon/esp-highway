#
# Makefile for esp-highway
#
# Start by setting the directories for the toolchain a few lines down
# the default target will build the firmware images
# `make flash` will flash the esp serially
# `VERBOSE=1 make ...` will print debug info
#
# Original from esp-link and others...
#VERBOSE=1

# --------------- toolchain configuration ---------------

# Base directory for the compiler. Needs a / at the end.
# Typically you'll install https://github.com/pfalcon/esp-open-sdk
XTENSA_TOOLS_ROOT ?= $(abspath ../esp-open-sdk/xtensa-lx106-elf/bin)/

# Base directory of the ESP8266 SDK package, absolute
# Typically you'll download from Espressif's BBS, http://bbs.espressif.com/viewforum.php?f=5
SDK_BASE	?= $(abspath ../esp_iot_sdk_v1.4.0)

# Esptool.py path and port, only used for 1-time serial flashing
# Typically you'll use https://github.com/themadinventor/esptool
# Windows users use the com port i.e: ESPPORT ?= com3
ESPTOOL		?= $(abspath ../esp-open-sdk/esptool/esptool.py)
ESPPORT		?= /dev/cu.usbserial-A800doxL
ESPBAUD		?= 460800

# --------------- chipset configuration   ---------------

# Pick your flash size: "512KB", "1MB", or "4MB"
FLASH_SIZE ?= 512KB

# The pin assignments below are used when the settings in flash are invalid, they
# can be changed via the web interface
# GPIO pin used to reset attached microcontroller, acative low
MCU_RESET_PIN       ?= 12
# GPIO pin used with reset to reprogram MCU (ISP=in-system-programming, unused with AVRs), active low
MCU_ISP_PIN         ?= 13
# GPIO pin used for "connectivity" LED, active low
LED_CONN_PIN        ?= 0
# GPIO pin used for "serial activity" LED, active low
LED_SERIAL_PIN      ?= 14

# --------------- esp-highway config options ---------------

# Optional Modules
MODULES ?=

ifeq ("$(FLASH_SIZE)","512KB")
# Winbond 25Q40 512KB flash, typ for esp-01 thru esp-11
ESP_SPI_SIZE        ?= 0       # 0->512KB (256KB+256KB)
ESP_FLASH_MODE      ?= 0       # 0->QIO
ESP_FLASH_FREQ_DIV  ?= 0       # 0->40Mhz
ESP_FLASH_MAX       ?= 241664  # max bin file for 512KB flash: 236KB
ET_FS               ?= 4m      # 4Mbit flash size in esptool flash command
ET_FF               ?= 40m     # 40Mhz flash speed in esptool flash command
ET_BLANK            ?= 0x7E000 # where to flash blank.bin to erase wireless settings

else ifeq ("$(FLASH_SIZE)","1MB")
# ESP-01E
ESP_SPI_SIZE        ?= 2       # 2->1MB (512KB+512KB)
ESP_FLASH_MODE      ?= 0       # 0->QIO
ESP_FLASH_FREQ_DIV  ?= 15      # 15->80MHz
ESP_FLASH_MAX       ?= 503808  # max bin file for 1MB flash: 492KB
ET_FS               ?= 8m      # 8Mbit flash size in esptool flash command
ET_FF               ?= 80m     # 80Mhz flash speed in esptool flash command
ET_BLANK            ?= 0xFE000 # where to flash blank.bin to erase wireless settings

else ifeq ("$(FLASH_SIZE)","2MB")
# Manuf 0xA1 Chip 0x4015 found on wroom-02 modules
# Here we're using two partitions of approx 0.5MB because that's what's easily available in terms
# of linker scripts in the SDK. Ideally we'd use two partitions of approx 1MB, the remaining 2MB
# cannot be used for code (esp8266 limitation).
ESP_SPI_SIZE        ?= 4       # 6->4MB (1MB+1MB) or 4->4MB (512KB+512KB)
ESP_FLASH_MODE      ?= 0       # 0->QIO, 2->DIO
ESP_FLASH_FREQ_DIV  ?= 15      # 15->80Mhz
ESP_FLASH_MAX       ?= 503808  # max bin file for 512KB flash partition: 492KB
#ESP_FLASH_MAX       ?= 1028096 # max bin file for 1MB flash partition: 1004KB
ET_FS               ?= 16m     # 16Mbit flash size in esptool flash command
ET_FF               ?= 80m     # 80Mhz flash speed in esptool flash command
ET_BLANK            ?= 0x1FE000 # where to flash blank.bin to erase wireless settings

else
# Winbond 25Q32 4MB flash, typ for esp-12
# Here we're using two partitions of approx 0.5MB because that's what's easily available in terms
# of linker scripts in the SDK. Ideally we'd use two partitions of approx 1MB, the remaining 2MB
# cannot be used for code (esp8266 limitation).
ESP_SPI_SIZE        ?= 4       # 6->4MB (1MB+1MB) or 4->4MB (512KB+512KB)
ESP_FLASH_MODE      ?= 0       # 0->QIO, 2->DIO
ESP_FLASH_FREQ_DIV  ?= 15      # 15->80Mhz
ESP_FLASH_MAX       ?= 503808  # max bin file for 512KB flash partition: 492KB
#ESP_FLASH_MAX       ?= 1028096 # max bin file for 1MB flash partition: 1004KB
ET_FS               ?= 32m     # 32Mbit flash size in esptool flash command
ET_FF               ?= 80m     # 80Mhz flash speed in esptool flash command
ET_BLANK            ?= 0x3FE000 # where to flash blank.bin to erase wireless settings
endif

# Output directors to store intermediate compiled files
# relative to the project directory
BUILD_BASE	= build
FW_BASE		= firmware

# name for the target project
TARGET		= highway

# espressif tool to concatenate sections for OTA upload using bootloader v1.2+
APPGEN_TOOL	?= gen_appbin.py

CFLAGS=

# which modules (subdirectories) of the project to include in compiling
LIBRARIES_DIR 	= libraries
MODULES		  	+= lib/uart lib/crc lib/httpd lib/cgi lib/wifi lib/mqtt
MODULES		  	+= src/cgi src/config src/mqtt_client src
MODULES			+= $(foreach sdir,$(LIBRARIES_DIR),$(wildcard $(sdir)/*))
EXTRA_INCDIR 	= include .

# libraries used in this project, mainly provided by the SDK
LIBS = c gcc hal phy pp net80211 wpa main lwip 

# compiler flags using during compilation of source files
CFLAGS	+= -Os -ggdb -std=c99 -Werror -Wpointer-arith -Wundef -Wall -Wl,-EL -fno-inline-functions \
		-nostdlib -mlongcalls -mtext-section-literals -ffunction-sections -fdata-sections \
		-D__ets__ -DICACHE_FLASH -D_STDINT_H -Wno-address -DFIRMWARE_SIZE=$(ESP_FLASH_MAX) \
		-DMCU_RESET_PIN=$(MCU_RESET_PIN) -DMCU_ISP_PIN=$(MCU_ISP_PIN) \
		-DLED_CONN_PIN=$(LED_CONN_PIN) -DLED_SERIAL_PIN=$(LED_SERIAL_PIN) \
		-DVERSION="$(VERSION)" -DMQTT -DCHANGE_TO_STA

# linker flags used to generate the main object file
LDFLAGS		= -nostdlib -Wl,--no-check-sections -u call_user_start -Wl,-static -Wl,--gc-sections

# linker script used for the above linker step
LD_SCRIPT 	:= build/eagle.esphttpd.v6.ld
LD_SCRIPT1	:= build/eagle.esphttpd1.v6.ld
LD_SCRIPT2	:= build/eagle.esphttpd2.v6.ld

# various paths from the SDK used in this project
SDK_LIBDIR		= lib
SDK_LDDIR		= ld
SDK_INCDIR		= include include/json
SDK_TOOLSDIR	= tools

# select which tools to use as compiler, librarian and linker
CC		:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-gcc
AR		:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-ar
LD		:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-gcc
OBJCP	:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-objcopy
OBJDP	:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-objdump


####
SRC_DIR		:= $(MODULES)
BUILD_DIR	:= $(addprefix $(BUILD_BASE)/,$(MODULES))

SDK_LIBDIR	:= $(addprefix $(SDK_BASE)/,$(SDK_LIBDIR))
SDK_LDDIR 	:= $(addprefix $(SDK_BASE)/,$(SDK_LDDIR))
SDK_INCDIR	:= $(addprefix -I$(SDK_BASE)/,$(SDK_INCDIR))
SDK_TOOLS	:= $(addprefix $(SDK_BASE)/,$(SDK_TOOLSDIR))
APPGEN_TOOL	:= $(addprefix $(SDK_TOOLS)/,$(APPGEN_TOOL))

SRC			:= $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.c))
OBJ			:= $(patsubst %.c,$(BUILD_BASE)/%.o,$(SRC))
LIBS		:= $(addprefix -l,$(LIBS))
APP_AR		:= $(addprefix $(BUILD_BASE)/,$(TARGET)_app.a)
USER1_OUT 	:= $(addprefix $(BUILD_BASE)/,$(TARGET).user1.out)
USER2_OUT 	:= $(addprefix $(BUILD_BASE)/,$(TARGET).user2.out)

INCDIR			:= $(addprefix -I,$(SRC_DIR))
EXTRA_INCDIR	:= $(addprefix -I,$(EXTRA_INCDIR))
MODULE_INCDIR	:= $(addsuffix /include,$(INCDIR))

V ?= $(VERBOSE)
ifeq ("$(V)","1")
Q :=
vecho := @true
else
Q := @
vecho := @echo
endif

vpath %.c $(SRC_DIR)

define compile-objects
$1/%.o: %.c
	$(vecho) "CC $$<"
	$(Q) $(CC) $(INCDIR) $(MODULE_INCDIR) $(EXTRA_INCDIR) $(SDK_INCDIR) $(CFLAGS)  -c $$< -o $$@
endef

.PHONY: all checkdirs clean wiflash

all: checkdirs $(FW_BASE)/user1.bin $(FW_BASE)/user2.bin

$(USER1_OUT): $(APP_AR) $(LD_SCRIPT1)
	$(vecho) "LD $@"
	$(Q) $(LD) -L$(SDK_LIBDIR) -T$(LD_SCRIPT1) $(LDFLAGS) -Wl,--start-group $(LIBS) $(APP_AR) -Wl,--end-group -o $@
	@echo Dump  : $(OBJDP) -x $(USER1_OUT)
	@echo Disass: $(OBJDP) -d -l -x $(USER1_OUT)
#	$(Q) $(OBJDP) -x $(TARGET_OUT) | egrep espfs_img

$(USER2_OUT): $(APP_AR) $(LD_SCRIPT2)
	$(vecho) "LD $@"
	$(Q) $(LD) -L$(SDK_LIBDIR) -T$(LD_SCRIPT2) $(LDFLAGS) -Wl,--start-group $(LIBS) $(APP_AR) -Wl,--end-group -o $@
#	$(Q) $(OBJDP) -x $(TARGET_OUT) | egrep espfs_img

$(FW_BASE):
	$(vecho) "FW $@"
	$(Q) mkdir -p $@

$(FW_BASE)/user1.bin: $(USER1_OUT) $(FW_BASE)
	$(Q) $(OBJCP) --only-section .text -O binary $(USER1_OUT) eagle.app.v6.text.bin
	$(Q) $(OBJCP) --only-section .data -O binary $(USER1_OUT) eagle.app.v6.data.bin
	$(Q) $(OBJCP) --only-section .rodata -O binary $(USER1_OUT) eagle.app.v6.rodata.bin
	$(Q) $(OBJCP) --only-section .irom0.text -O binary $(USER1_OUT) eagle.app.v6.irom0text.bin
	ls -ls eagle*bin
	$(Q) COMPILE=gcc PATH=$(XTENSA_TOOLS_ROOT):$(PATH) python $(APPGEN_TOOL) $(USER1_OUT) 2 $(ESP_FLASH_MODE) $(ESP_FLASH_FREQ_DIV) $(ESP_SPI_SIZE)
	$(Q) rm -f eagle.app.v6.*.bin
	$(Q) mv eagle.app.flash.bin $@
	@echo "** user1.bin uses $$(gstat -c '%s' $@) bytes of" $(ESP_FLASH_MAX) "available"
	$(Q) if [ $$(gstat -c '%s' $@) -gt $$(( $(ESP_FLASH_MAX) )) ]; then echo "$@ too big!"; false; fi

$(FW_BASE)/user2.bin: $(USER2_OUT) $(FW_BASE)
	$(Q) $(OBJCP) --only-section .text -O binary $(USER2_OUT) eagle.app.v6.text.bin
	$(Q) $(OBJCP) --only-section .data -O binary $(USER2_OUT) eagle.app.v6.data.bin
	$(Q) $(OBJCP) --only-section .rodata -O binary $(USER2_OUT) eagle.app.v6.rodata.bin
	$(Q) $(OBJCP) --only-section .irom0.text -O binary $(USER2_OUT) eagle.app.v6.irom0text.bin
	$(Q) COMPILE=gcc PATH=$(XTENSA_TOOLS_ROOT):$(PATH) python $(APPGEN_TOOL) $(USER2_OUT) 2 $(ESP_FLASH_MODE) $(ESP_FLASH_FREQ_DIV) $(ESP_SPI_SIZE)
	$(Q) rm -f eagle.app.v6.*.bin
	$(Q) mv eagle.app.flash.bin $@
	$(Q) if [ $$(gstat -c '%s' $@) -gt $$(( $(ESP_FLASH_MAX) )) ]; then echo "$@ too big!"; false; fi

$(APP_AR): $(OBJ)
	$(vecho) "AR $@"
	$(Q) $(AR) cru $@ $^

checkdirs: $(BUILD_DIR)

$(BUILD_DIR):
	$(Q) mkdir -p $@

baseflash: all
	$(Q) $(ESPTOOL) --port $(ESPPORT) --baud $(ESPBAUD) write_flash 0x01000 $(FW_BASE)/user1.bin

flash: all
	$(Q) $(ESPTOOL) --port $(ESPPORT) --baud $(ESPBAUD) write_flash -fs $(ET_FS) -ff $(ET_FF) \
	  0x00000 "$(SDK_BASE)/bin/boot_v1.4(b1).bin" 0x01000 $(FW_BASE)/user1.bin \
	  $(ET_BLANK) $(SDK_BASE)/bin/blank.bin
	  
# edit the loader script to add the espfs section to the end of irom with a 4 byte alignment.
# we also adjust the sizes of the segments 'cause we need more irom0
# in the end the only thing that matters wrt size is that the whole shebang fits into the
# 236KB available (in a 512KB flash)
ifeq ("$(FLASH_SIZE)","512KB")
build/eagle.esphttpd1.v6.ld: $(SDK_LDDIR)/eagle.app.v6.new.512.app1.ld
	$(Q) sed -e '/\.irom\.text/{' -e 'a . = ALIGN (4);' -e 'a *(.espfs)' -e '}'  \
			-e '/^  irom0_0_seg/ s/2B000/38000/' \
			$(SDK_LDDIR)/eagle.app.v6.new.512.app1.ld >$@
build/eagle.esphttpd2.v6.ld: $(SDK_LDDIR)/eagle.app.v6.new.512.app2.ld
	$(Q) sed -e '/\.irom\.text/{' -e 'a . = ALIGN (4);' -e 'a *(.espfs)' -e '}'  \
			-e '/^  irom0_0_seg/ s/2B000/38000/' \
			$(SDK_LDDIR)/eagle.app.v6.new.512.app2.ld >$@
else
build/eagle.esphttpd1.v6.ld: $(SDK_LDDIR)/eagle.app.v6.new.1024.app1.ld
	$(Q) sed -e '/\.irom\.text/{' -e 'a . = ALIGN (4);' -e 'a *(.espfs)' -e '}'  \
			-e '/^  irom0_0_seg/ s/6B000/7C000/' \
			$(SDK_LDDIR)/eagle.app.v6.new.1024.app1.ld >$@
build/eagle.esphttpd2.v6.ld: $(SDK_LDDIR)/eagle.app.v6.new.1024.app2.ld
	$(Q) sed -e '/\.irom\.text/{' -e 'a . = ALIGN (4);' -e 'a *(.espfs)' -e '}'  \
			-e '/^  irom0_0_seg/ s/6B000/7C000/' \
			$(SDK_LDDIR)/eagle.app.v6.new.1024.app2.ld >$@
endif

clean:
	$(Q) rm -f $(APP_AR)
	$(Q) rm -f $(TARGET_OUT)
	$(Q) find $(BUILD_BASE) | xargs rm -rf
	$(Q) rm -rf $(FW_BASE)

$(foreach bdir,$(BUILD_DIR),$(eval $(call compile-objects,$(bdir))))
