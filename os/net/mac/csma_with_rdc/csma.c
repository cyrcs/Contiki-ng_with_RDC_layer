/*
 * Copyright (c) 2010, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \file
 *         The 802.15.4 standard CSMA protocol (nonbeacon-enabled)
 * \author
 *         Adam Dunkels <adam@sics.se>
 *         Simon Duquennoy <simon.duquennoy@inria.fr>
 */

#include "net/mac/csma_with_rdc/csma.h"
#include "net/mac/csma_with_rdc/csma-output.h"
#include "net/mac/mac-sequence.h"
#include "net/packetbuf.h"
#include "net/netstack.h"
#include "net/mac/rdc/rdc.h"

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "CSMA (RDC)"
#define LOG_LEVEL LOG_LEVEL_MAC

static int on(void);
static int off(void);
static int max_payload(void);

/*---------------------------------------------------------------------------*/
static void
init(void)
{
  LOG_FUNC("Function call: %s\n", __func__);
  radio_value_t radio_max_payload_len;

  /* Check that the radio can correctly report its max supported payload */
  if (NETSTACK_RADIO.get_value(RADIO_CONST_MAX_PAYLOAD_LEN, &radio_max_payload_len) != RADIO_RESULT_OK)
  {
    LOG_ERR("! radio does not support getting RADIO_CONST_MAX_PAYLOAD_LEN. Abort init.\n");
    return;
  }

#if CSMA_SEND_SOFT_ACK
  radio_value_t radio_rx_mode;

  /* Disable radio driver's autoack */
  if (NETSTACK_RADIO.get_value(RADIO_PARAM_RX_MODE, &radio_rx_mode) != RADIO_RESULT_OK)
  {
    LOG_WARN("radio does not support getting RADIO_PARAM_RX_MODE\n");
  }
  else
  {
    /* Unset autoack */
    radio_rx_mode &= ~RADIO_RX_MODE_AUTOACK;
    if (NETSTACK_RADIO.set_value(RADIO_PARAM_RX_MODE, radio_rx_mode) != RADIO_RESULT_OK)
    {
      LOG_WARN("radio does not support setting RADIO_PARAM_RX_MODE\n");
    }
  }
#endif

  mac_sequence_init();

#if LLSEC802154_USES_AUX_HEADER
#ifdef CSMA_LLSEC_DEFAULT_KEY0
  uint8_t key[16] = CSMA_LLSEC_DEFAULT_KEY0;
  csma_security_set_key(0, key);
#endif
#endif /* LLSEC802154_USES_AUX_HEADER */
  csma_output_init();
  on();
}
/*---------------------------------------------------------------------------*/
static void
init_sec(void)
{
  LOG_FUNC("Function call: %s\n", __func__);
#if LLSEC802154_USES_AUX_HEADER
  if (packetbuf_attr(PACKETBUF_ATTR_SECURITY_LEVEL) ==
      PACKETBUF_ATTR_SECURITY_LEVEL_DEFAULT)
  {
    packetbuf_set_attr(PACKETBUF_ATTR_SECURITY_LEVEL,
                       CSMA_LLSEC_SECURITY_LEVEL);
  }
#endif
}
/*---------------------------------------------------------------------------*/
static void
send_packet(mac_callback_t sent, void *ptr)
{

  LOG_FUNC("Function call: %s\n", __func__);

  init_sec();

  csma_output_packet(sent, ptr);
}
/*---------------------------------------------------------------------------*/
static void
input_packet(void)
{
  LOG_FUNC("Function call: %s\n", __func__);
  NETSTACK_NETWORK.input();
}
/*---------------------------------------------------------------------------*/
static int
on(void)
{
  LOG_FUNC("Function call: %s\n", __func__);
  return NETSTACK_RDC.on();
}
/*---------------------------------------------------------------------------*/
static int
off(void)
{
  LOG_FUNC("Function call: %s\n", __func__);
  return NETSTACK_RDC.off();
}
/*---------------------------------------------------------------------------*/

static int
max_payload(void)
{
  LOG_FUNC("Function call: %s\n", __func__);
  int framer_hdrlen;
  radio_value_t max_radio_payload_len;
  radio_result_t res;

  init_sec();

  framer_hdrlen = NETSTACK_FRAMER.length();

  res = NETSTACK_RADIO.get_value(RADIO_CONST_MAX_PAYLOAD_LEN,
                                 &max_radio_payload_len);

  if (res == RADIO_RESULT_NOT_SUPPORTED)
  {
    LOG_ERR("Failed to retrieve max radio driver payload length\n");
    return 0;
  }

  if (framer_hdrlen < 0)
  {
    /* Framing failed, we assume the maximum header length */
    framer_hdrlen = CSMA_MAC_MAX_HEADER;
  }

  return MIN(max_radio_payload_len, PACKETBUF_SIZE) - framer_hdrlen - LLSEC802154_PACKETBUF_MIC_LEN();
}

/*---------------------------------------------------------------------------*/
const struct mac_driver csma_with_rdc_driver = {
    "CSMA WITH RDC",
    init,
    send_packet,
    input_packet,
    on,
    off,
    max_payload,
};
/*---------------------------------------------------------------------------*/
