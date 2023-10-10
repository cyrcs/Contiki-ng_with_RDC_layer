# General overview of the project

## TODO 
- go through every file and explain everything in comments
- implement RDC layer in contiki-ng
- compare RDC protocols: ContikiMAC, X-MAC, LPP, others

## improving the protocol
- test contikiMAC with different timings
- Test CAD with different amount of symbols
- Automatically change SF, BW based on data to send
- Timings will depend on the SF and BW selected
- A phase lock system can be added, as well as a "listen before talk" feature.

## Ideas
- create new pcb shield => required changes:
  - swap pins specified in report

# Literature
## LoRa
- 2.4 GHz
- LoRaWAN uses a Aloha like MAC protocol, 1 hop star topology, stay awake until ACK
- 1% duty cycle limit
- sub noise transmission
- LoRaWAN is bad for scalability, coverage, data rate, gateway outage => multihop
- uses orthogonal SF instead of orthogonal spreading codes
- SF between 7-12: higher SF more range, lower data rate, longer air time
- 2.4 GHz has no duty cycle restriction => more collisions => need good RDC
- 2.4 GHz has B increased to a max of 1600 kHz
- CAD: Clear Activity Detection. Can detect LoRa signals below the noise level
- USED SETTINGS: SF 7, BW 403 kHz, Preamble length 8, CR 4/5, CRC true

## Hardware
- zolertia firefly boards
  - used pinout: SCK B2, MISO B3, MOSI B1, CS A3, RESET A2, DIO1 A5, BUSY A4
- hardware shield with sx1280 LoRa transceiver




# Report
## Concepts to talk about
- LoRa
- IoT

## My work
- Integrate driver into contiki-ng
- change file structure to contain #region
- add RDC layer to contiki-ng

# Detailed list of **changes**
## Implementation of driver in contiki-ng:
- place src folder inside arch/dev/radio/sx1280
- place examples in examples/sx1280
- changes in contiki files:
  - tsch-const.h 82: add #ifndef around TSCH_PACKET_DURATION
  - tsch_types.h 145: change tsch_timeslot_timing_usec to: \
  #if NETSTACK_CONF_RADIO == sx128x_radio_driver \
typedef uint32_t tsch_timeslot_timing_usec[tsch_ts_elements_count];\
#else\
typedef uint16_t tsch_timeslot_timing_usec[tsch_ts_elements_count];\
#endif
   
  - cc1200-rf-cfg.h 100: change uint16 to uint32

## Implementation of RDC layer in contiki-ng
- netstack.h:
  - at line 93 add:\
  /* RDC layer configuration. The RDC layer is configured through the Makefile,
   via the flag MAKE_RDC */\
#ifdef NETSTACK_CONF_RDC\
#define NETSTACK_RDC NETSTACK_CONF_RDC\
#else\
#if RDC_CONF_WITH_NORDC\
#define NETSTACK_RDC nordc_driver\
#elif RDC_CONF_WITH_NULLRDC\
#define NETSTACK_RDC nullrdc_driver\
#elif RDC_CONF_WITH_NULLRDC_NOFRAMER\
#define NETSTACK_RDC nullrdc_noframer_driver\
#else\
#error Unknown RDC configuration\
#endif\
#endif\

    #ifndef NETSTACK_RDC_CHANNEL_CHECK_RATE\
    #ifdef NETSTACK_CONF_RDC_CHANNEL_CHECK_RATE\
    #define NETSTACK_RDC_CHANNEL_CHECK_RATE NETSTACK_CONF_RDC_CHANNEL_CHECK_RATE\
    #else /* NETSTACK_CONF_RDC_CHANNEL_CHECK_RATE */\
    #define NETSTACK_RDC_CHANNEL_CHECK_RATE 8\
    #endif /* NETSTACK_CONF_RDC_CHANNEL_CHECK_RATE */\
    #endif /* NETSTACK_RDC_CHANNEL_CHECK_RATE */

    #if (NETSTACK_RDC_CHANNEL_CHECK_RATE & (NETSTACK_RDC_CHANNEL_CHECK_RATE - 1)) != 0\
    #error NETSTACK_RDC_CONF_CHANNEL_CHECK_RATE must be a power of two (i.e., 1, 2, 4, 8, 16, 32, 64, ...).\
    #error Change NETSTACK_RDC_CONF_CHANNEL_CHECK_RATE in contiki-conf.h, project-conf.h or in your Makefile.\
    #endif
  - line 142 add: \
  #include "net/mac/rdc/rdc.h"
  - line 164 add: \
extern const struct rdc_driver NETSTACK_RDC;
  - line 172 add: \
  NETSTACK_RDC.init(); 
  
- makefile.include Line 294:\
    \# Configure RDC layer\
    MAKE_RDC_NORDC = 0\
    MAKE_RDC_NULLRDC = 1\
    MAKE_RDC_NULLRDC_NOFRAMER = 2

    \# Make NullRDC the default RDC\
    MAKE_RDC ?= MAKE_RDC_NULLRDC_NOFRAMER

    ifeq (\$(MAKE\_RDC),MAKE\_RDC\_NORDC)\
      MODULES += \$(CONTIKI\_NG_RDC_DIR)\
      CFLAGS += -DRDC_CONF_WITH_NORDC=1\
    endif\
    ifeq (\$(MAKE_RDC),MAKE_RDC_NULLRDC)\
      MODULES += \$(CONTIKI_NG_RDC_DIR)/nullrdc\
      CFLAGS += -DRDC_CONF_WITH_NULLRDC=1\
    endif\
    ifeq (\$(MAKE_RDC),MAKE_RDC_NULLRDC_NOFRAMER)\
      MODULES += $(CONTIKI_NG_RDC_DIR)/nullrdc-noframer\
      CFLAGS += -DRDC_CONF_WITH_NULLRDC_NOFRAMER=1\
    endif

- makefile.dir-variables Line 16:
  - CONTIKI_NG_RDC_DIR = $(CONTIKI_NG_NET_DIR)/mac/rdc
  
- Allow Logs for RDC layer:
  - log.c Line 62:
    - int curr_log_level_rdc = LOG_CONF_LEVEL_RDC;
  - log.c Line 77:
    - {"rdc", &curr_log_level_rdc, LOG_CONF_LEVEL_RDC},
  - log.h line 115:
    - extern int curr_log_level_rdc;
  - log.h line 131:
    - #define LOG_LEVEL_RDC MIN((LOG_CONF_LEVEL_RDC), curr_log_level_rdc)
  - log-conf.h line 136:
    - #ifndef LOG_CONF_LEVEL_RDC\
      #define LOG_CONF_LEVEL_RDC                         LOG_LEVEL_NONE\
      #endif /* LOG_CONF_LEVEL_RDC */
  

- created files:
  - rdc.h: contains general structure for a RDC driver
  - nordc.c: implementation of **nordc**
  - nullrdc-framer.h
  - nullrdc-framer.c