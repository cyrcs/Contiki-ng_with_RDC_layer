CONTIKI_PROJECT = receiver

all: $(CONTIKI_PROJECT)

TARGET = zoul
BOARD = firefly
PORT=/dev/ttyUSB0

CONTIKI = $(abspath ../../..)

include $(CONTIKI)/Makefile.dir-variables
MODULES += $(CONTIKI_NG_DRIVERS_RADIO_DIR)/sx128x
MODULES += $(CONTIKI_NG_SERVICES_DIR)/shell
MODULES += os/lib/newlib

CFLAGS += -I/usr/arm-none-eabi/include
CFLAGS += -Wno-error=array-bounds
CFLAGS += -Wno-array-bounds

MAKE_MAC = MAKE_MAC_CSMA
MAKE_RDC = MAKE_RDC_NULLRDC
MAKE_NET = MAKE_NET_NULLNET

include $(CONTIKI)/Makefile.include

# make clean && make sx1280_recv.upload && make login