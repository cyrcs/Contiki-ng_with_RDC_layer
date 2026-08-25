#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

#define NETSTACK_CONF_RDC xmac_driver

/* Radio: overschakelen van interne CC2538 naar de externe LAMBDA80 (SX1280) */
#define NETSTACK_CONF_RADIO sx128x_radio_driver

/* Soft-ack macro: de sx128x-driver checkt CSMA_CONF_SEND_SOFT_ACK,
   terwijl xmac.c/csma.c CSMA_SEND_SOFT_ACK gebruiken. Beide nodig. */
//#define CSMA_SEND_SOFT_ACK 1
#define CSMA_CONF_SEND_SOFT_ACK 1

/* LoRa: start met SF7 als eerste werkende baseline (kortste time-on-air) */
#define CONFIG_LORA24_SF_DEFAULT LORA_SF_7

#define LOG_CONF_LEVEL_RDC LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_MAC LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_RADIO LOG_LEVEL_INFO
#define XMAC_CONF_DEBUG_LEDS 1

#define XMAC_CONF_USE_CACHE 1
#define ENERGEST_CONF_ON 0

#endif /* PROJECT_CONF_H_ */