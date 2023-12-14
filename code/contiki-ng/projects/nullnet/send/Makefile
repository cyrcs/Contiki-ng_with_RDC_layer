CONTIKI_PROJECT = nullnet-unicast nullnet-broadcast
all: $(CONTIKI_PROJECT)
CONTIKI = ../../..

TARGET=zoul
BOARD=firefly-reva
PORT=/dev/ttyUSB0

include $(CONTIKI)/Makefile.dir-variables
MODULES += $(CONTIKI_NG_DRIVERS_RADIO_DIR)/sx128x
MODULES += $(CONTIKI_NG_SERVICES_DIR)/shell
CFLAGS += -I/usr/arm-none-eabi/include

MAKE_MAC = MAKE_MAC_CSMA_WITH_RDC
MAKE_NET = MAKE_NET_NULLNET
MAKE_RDC = MAKE_RDC_NULLRDC

include $(CONTIKI)/Makefile.include
