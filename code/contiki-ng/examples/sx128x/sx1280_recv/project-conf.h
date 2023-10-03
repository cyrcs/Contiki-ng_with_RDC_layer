/*---------------------------------------------------------------------------*/
/**
 * \addtogroup zoul-examples
 * @{
 *
 * \file
 * Project specific configuration defines for the basic RE-Mote examples
 */
/*---------------------------------------------------------------------------*/
#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

#include <stddef.h>

#define NETSTACK_CONF_RADIO                        sx128x_radio_driver

extern int tsch_packet_duration(size_t len); 
#define TSCH_PACKET_DURATION(len) tsch_packet_duration(len) 

//MAC+RDC
//#define NETSTACK_CONF_RDC_CHANNEL_CHECK_RATE 16
//#define NETSTACK_CONF_RDC nullrdc_driver
//#define NETSTACK_CONF_RDC contikimac_driver
//names of RDC protocols:
/*
 contikimac_driver
 cxmac_driver
 nullrdc_driver
 */
//uses csma since nullmac_driver gives "undefined errors"

//csma driver does not seem to do anything for the lora module. 
//no MAC_LEVEL messages are observerd although log level DBG
//#define NETSTACK_CONF_MAC csma_driver
//#define NETSTACK_CONF_MAC nullmac_driver

/* Logging */
//LEVELS: NONE,ERR,WARN,INFO,DBG
#define LOG_CONF_LEVEL_RPL                         LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_TCPIP                       LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_IPV6                        LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_6LOWPAN                     LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_MAC                         LOG_LEVEL_DBG
#define LOG_CONF_LEVEL_FRAMER                      LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_RN2483                      LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_RN2483_UART                 LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_TSCH                        LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_TSCH_LOG                    LOG_LEVEL_WARN
#define TSCH_LOG_CONF_PER_SLOT                     1

#endif /* PROJECT_CONF_H_ */

/** @} */
