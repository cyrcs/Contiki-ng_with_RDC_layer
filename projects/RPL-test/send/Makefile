all: dis-sender sender-node receiver-node root-node
CONTIKI=../../..

TARGET=zoul
BOARD=firefly-reva
PORT=/dev/ttyUSB0

include $(CONTIKI)/Makefile.dir-variables
MODULES += $(CONTIKI_NG_DRIVERS_RADIO_DIR)/sx128x
MODULES += $(CONTIKI_NG_SERVICES_DIR)/shell
CFLAGS += -I/usr/arm-none-eabi/include

MAKE_ROUTING = MAKE_ROUTING_RPL_CLASSIC
MAKE_MAC = MAKE_MAC_CSMA
# MAKE_RDC = MAKE_RDC_NULLRDC
# MAKE_NET = MAKE_NET_NULLNET

include $(CONTIKI)/Makefile.include



