CONTIKI_PROJECT = nullnet-unicast nullnet-broadcast
all: $(CONTIKI_PROJECT)
CONTIKI = ../../..

TARGET=zoul
BOARD=firefly-reva
PORT=/dev/ttyUSB1

include $(CONTIKI)/Makefile.dir-variables
MODULES += $(CONTIKI_NG_DRIVERS_RADIO_DIR)/sx128x
MODULES += $(CONTIKI_NG_SERVICES_DIR)/shell
CFLAGS += -I/usr/arm-none-eabi/include

# MAKE_MAC = MAKE_MAC_CSMA
MAKE_MAC = MAKE_MAC_CSMA_WITH_RDC
MAKE_RDC = MAKE_RDC_XMAC
# MAKE_RDC = MAKE_RDC_NULLRDC
MAKE_NET = MAKE_NET_NULLNET

include $(CONTIKI)/Makefile.include

# make clean && make nullnet-unicast.upload && make login