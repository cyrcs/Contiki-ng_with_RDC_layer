all: dis-sender sender-node receiver-node root-node
CONTIKI=../../..

TARTET=zoul
BOARD=firefly-reva

include $(CONTIKI)/Makefile.dir-variables
MODULES += $(CONTIKI_NG_DRIVERS_RADIO_DIR)/sx128x
MODULES += $(CONTIKI_NG_SERVICES_DIR)/shell
CFLAGS += -I/usr/arm-none-eabi/include

MAKE_ROUTING = MAKE_ROUTING_RPL_CLASSIC
# MAKE_NET = MAKE_NET_NULLNET
# MAKE_MAC = MAKE_MAC_CSMA
# MAKE_RDC = MAKE_RDC_NULLRDC

include $(CONTIKI)/Makefile.include



