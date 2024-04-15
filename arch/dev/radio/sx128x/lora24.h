/*
 * Copyright (C) 2017 Inria
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    net_lora LoRa modulation
 * @ingroup     net
 * @brief       LoRa modulation header definitions
 * @{lora24
 * @file
 * @brief       LoRa modulation header definitions
 *
 * @author      Alexandre Abadie <alexandre.abadie@inria.fr>
 */

#ifndef NET_LORA24_H
#define NET_LORA24_H

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup net_lora_conf  LoRa 2.4GHz modulation compile configurations
 * @ingroup  config
 * @{
 */

/** @brief Preamble length, same for Tx and Rx
 *
 * Configure preamble used in LoRa frame. Each LoRa frame begins with a
 * preamble. It starts with a series of upchirps to cover the whole frequency
 * band of the particular channel assigned. The last two upchirps encode the
 * sync word. Sync word is used to differentiate between LoRa transmissions that
 * use the same frequency bands. The sync word is followed by two and a quarter
 * downchirps, for a duration of 2.25 symbols. The total duration of this
 * preamble can be configured between 10.25 and 65,539.25 symbol hence the value
1* can range from 8 to 65537.
*/
#ifndef CONFIG_LORA24_PREAMBLE_LENGTH_DEFAULT
#define CONFIG_LORA24_PREAMBLE_LENGTH_DEFAULT         (15U)
#endif

/** @brief Set Spreading Factor (SF)
 *
 * Configure Spreading Factor (SF). SF denotes the amount of spreading code
 * applied to the original data signal. A larger SF increases the time on air,
 * which increases energy consumption, reduces the data rate, and improves
 * communication range. Each step up in spreading factor effectively doubles the
 * time on air to transmit the same amount of data. Refer to country specific
 * air time usage regulations before varying the SF.
*/

#ifndef CONFIG_LORA24_SF_DEFAULT
#define CONFIG_LORA24_SF_DEFAULT                      (LORA_SF_12)
#endif


/** @brief Set channel bandwidth
 *
 * Configure the channel bandwidth. Refer to country specific regulation on
 * channel usage to identify the correct bandwidth.
*/

#ifndef CONFIG_LORA24_BW_DEFAULT
#define CONFIG_LORA24_BW_DEFAULT                      (LORA_BW_400)
#endif


/** @brief Set Coding Rate (CR)
 *
 * Configure Coding Rate (CR). CR denotes the implementation of forward error
 * correction (FEC). This may be done by encoding 4-bit data with redundancies
 * into 5-bit, 6-bit, 7-bit, or 8-bit. Coding Rate (CR) value need to be
 * adjusted according to conditions of the channel used for data transmission.
 * If there are too many interferences in the channel, then it’s recommended to
 * increase the value of CR. However, the rise in CR value will also increase
 * the duration for the transmission. Refer to country specific air time usage
 * regulations before varying the CR. To calculate air time refer
 * https://www.loratools.nl/#/airtime .
*/
#ifndef CONFIG_LORA24_CR_DEFAULT
#define CONFIG_LORA24_CR_DEFAULT                      (LORA_CR_4_5)
#endif

#ifndef CONFIG_LORA24_CAD_SYMBOLS_DEFAULT
#define CONFIG_LORA24_CAD_SYMBOLS_DEFAULT             (CAD_SYMBOLS_01)
#endif


/** @brief Configure payload length
 *
 * Configure the length of payload. The configuration is unused when using
 * explicit header mode ( @ref CONFIG_LORA_FIXED_HEADER_LEN_MODE_DEFAULT ) as
 * `PHDR` carries the length information.
*/
#ifndef CONFIG_LORA24_PAYLOAD_LENGTH_DEFAULT
#define CONFIG_LORA24_PAYLOAD_LENGTH_DEFAULT          (0U)
#endif
/** @} */

// ! LoRa flags

/** @brief Set this to 1 to enable inverted I/Q mode
 *
 * Enable this to invert the IQ signals used in RF modulation circuit. For more
 * information on I/Q modulation technique visit http://www.ni.com/tutorial/4805/en/
*/
#ifdef DOXYGEN
#define CONFIG_LORA24_IQ_INVERTED_DEFAULT
#endif

/** @brief Set this to 1 to enable fixed header length mode (implicit header)
 *
 * If fixed header length mode ( implicit header mode) is enabled, PHY header
 * (`PHDR`) in LoRa frame is discarded. For more information, refer to the
 * section "LoRa frame structure" in this
 * <a href="https://link.springer.com/article/10.1186/s13638-019-1542-x">publication</a>
*/
#ifdef DOXYGEN
#define CONFIG_LORA24_FIXED_HEADER_LEN_MODE_DEFAULT
#endif

/** @brief Enable/disable payload CRC, optional
 *
 * @deprecated Use inverse `CONFIG_LORA_PAYLOAD_CRC_OFF_DEFAULT` instead.
 * Will be removed after 2021.04 release.
*/
#ifndef CONFIG_LORA24_PAYLOAD_CRC_ON_DEFAULT
#define CONFIG_LORA24_PAYLOAD_CRC_ON_DEFAULT                 (true)
#endif

// ! below are the unused settings

/** @brief Frequency resolution in Hz */
#ifndef LORA24_FREQUENCY_RESOLUTION_DEFAULT
#define LORA24_FREQUENCY_RESOLUTION_DEFAULT      (198.3642578)
#endif

/** @brief Symbol timeout period in symbols
 *
 * Configure symbol time out in terms of number of symbols. One symbol has a
 * length in time of (2^SF)/BW seconds.
*/ // ! shouldn't this be a calcuation based on the SF and BW?
#ifndef CONFIG_LORA24_SYMBOL_TIMEOUT_DEFAULT
#define CONFIG_LORA24_SYMBOL_TIMEOUT_DEFAULT          (10U)
#endif

/**
 * @name    LoRa syncword values for network types
 * @{
 */
#define LORA24_SYNCWORD_PUBLIC (0x34)  /**< Syncword used for public networks */
#define LORA24_SYNCWORD_PRIVATE (0x12)  /**< Syncword used for private networks */
/** @} */

// ! LoRa convertion functions

#define LORA24_BW_TO_KHZ(x) ((1 << x) * 200)
#define CEILING_POS(X) ((X-(int)(X)) > 0 ? (int)(X+1) : (int)(X))
#define LORA_T_SYM_USEC(sf, bw) ((1 << (sf)) * 1000 / (LORA24_BW_TO_KHZ(bw)))
#define LORA_SYM_NB(sf, bw, crc, header, cr, prlen, len) ( \
  prlen + \
   ((sf < 7) ? 6.25 : 4.25) + 8 + (CEILING_POS( \
    MAX((float) ( (8 * (len)) - (4 * (sf)) + ((sf < 7) ? 0 : 8) + (16 * (crc)) + (20 * (header)) ), 0) \
    / ((sf < 11) ? (4 * sf) : (4 * (sf - 2)))) \
   * ((cr) + 4)) \
)
#define LORA_T_PACKET_USEC(sf, bw, crc, header, cr, prlen, len) ( \
  LORA_SYM_NB(sf, bw, crc, header, cr, prlen, len) * LORA_T_SYM_USEC(sf, bw) \
)

#ifdef __cplusplus
}
#endif

#endif /* NET_LORA24_H */
/** @} */
