CONTIKI_PROJECT = xmac_test
all: $(CONTIKI_PROJECT)

TARGET = zoul
BOARD = firefly

CONTIKI = ../../../../../

include $(CONTIKI)/Makefile.dir-variables
MODULES += $(CONTIKI_NG_DRIVERS_RADIO_DIR)/sx128x
MODULES += os/net/mac/rdc/xmac
MODULES += os/net/mac/csma_with_rdc

CFLAGS += -Wno-error=array-bounds
CFLAGS += -Wno-array-bounds

MAKE_MAC = MAKE_MAC_NULLMAC
MAKE_NET = MAKE_NET_NULLNET

include $(CONTIKI)/Makefile.include
