CONTIKI_PROJECT = sx1280_recv

all: $(CONTIKI_PROJECT)

TARGET = zoul
BOARD = firefly
PORT = /dev/ttyUSB1

CONTIKI = ../../..
#CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"
#guide: https://github.com/contiki-os/contiki/wiki/Change-mac-or-radio-duty-cycling-protocols
#not needed to incluede project-conf.h since already included
include $(CONTIKI)/Makefile.dir-variables
#MODULES += ../src/ # SX128X Driver
MODULES += $(CONTIKI_NG_DRIVERS_RADIO_DIR)/sx128x
MODULES += $(CONTIKI_NG_SERVICES_DIR)/shell
CFLAGS += -I/usr/arm-none-eabi/include

#defined in project-conf.h
MAKE_MAC = MAKE_MAC_NULLMAC
MAKE_RDC = MAKE_RDC_NULLRDC_NOFRAMER
MAKE_NET = MAKE_NET_NULLNET


include $(CONTIKI)/Makefile.include
