CONTIKI_PROJECT = nullnet-unicast nullnet-broadcast
all: $(CONTIKI_PROJECT)

TARGET=zoul
BOARD=firefly-reva
PORT=/dev/ttyUSB0

CONTIKI = ../..

#use this to enable TSCH: MAKE_MAC = MAKE_MAC_TSCH
# MAKE_MAC ?= MAKE_MAC_CSMA
MAKE_NET = MAKE_NET_NULLNET
include $(CONTIKI)/Makefile.include
