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
 *         A null RDC implementation that uses framer for headers.
 * \author
 *         Adam Dunkels <adam@sics.se>
 *         Niclas Finne <nfi@sics.se>
 */

#include "net/mac/rdc/nullrdc/nullrdc.h"
#include "net/packetbuf.h"
#include "net/queuebuf.h"
#include "net/netstack.h"
#include <string.h>

// TODO find a better way to include to required functions/constants
#include "net/mac/mac-sequence.h"
#include "net/mac/csma_with_rdc/csma-security.h"
#include "net/mac/csma_with_rdc/csma.h"

#include "dev/watchdog.h"
#include "sys/ctimer.h"
#include "sys/rtimer.h"

#include "sys/clock.h"
#include "lib/random.h"
#include "lib/list.h"
#include "lib/memb.h"
#include "lib/assert.h"

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "NULLRDC"
#define LOG_LEVEL LOG_LEVEL_RDC

#ifndef RECEIVER
#define RECEIVER 0
#endif

#include "sys/rtimer.h"
#include "dev/watchdog.h"

#define ACK_LEN 3

bool packet_received;
bool radio_on;

/*---------------------------------------------------------------------------*/
static int on(void);

/*---------------------------------------------------------------------------*/
static void
init(void)
{
  LOG_DBG("Function call: %s\n", __func__);
  on();
}
/*---------------------------------------------------------------------------*/
static int
send_one_packet(mac_callback_t sent, void *ptr)
{
  LOG_DBG("Function call: %s\n", __func__);
  // create 2 required variables
  int ret;
  int last_sent_ok = 0;

  // Create packet header
  // first add node addr
  packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &linkaddr_node_addr);
  packetbuf_set_attr(PACKETBUF_ATTR_MAC_ACK, 1);

  // This code is used in contiki-ng to enable LLSEC
  // TODO check if still requires additional changes
#if LLSEC802154_ENABLED
#if LLSEC802154_USES_EXPLICIT_KEYS
  /* This should possibly be taken from upper layers in the future */
  packetbuf_set_attr(PACKETBUF_ATTR_KEY_ID_MODE, CSMA_LLSEC_KEY_ID_MODE);
#endif /* LLSEC802154_USES_EXPLICIT_KEYS */
#endif /* LLSEC802154_ENABLED */

  // TODO contiki-ng uses a csma security function to create the header
  // QUESTION: is it allowed to used MAC layer functions inside the RDC layer?
  // the ELSE statement has to be changed

  if (csma_security_create_frame() < 0)
  {
    /* Failed to allocate space for headers */
    LOG_ERR("failed to create packet, seqno: %d\n", packetbuf_attr(PACKETBUF_ATTR_MAC_SEQNO));
    ret = MAC_TX_ERR_FATAL;
  }
  else
  {
    int is_broadcast;
    uint8_t dsn;
    dsn = ((uint8_t *)packetbuf_hdrptr())[2] & 0xff;

    NETSTACK_RADIO.prepare(packetbuf_hdrptr(), packetbuf_totlen());

    is_broadcast = packetbuf_holds_broadcast();

    if (NETSTACK_RADIO.receiving_packet() ||
        (!is_broadcast && NETSTACK_RADIO.pending_packet()))
    {

      /* Currently receiving a packet over air or the radio has
         already received a packet that needs to be read before
         sending with auto ack. */
      ret = MAC_TX_COLLISION;
    }
    else
    {
      switch (NETSTACK_RADIO.transmit(packetbuf_totlen()))
      {
      case RADIO_TX_OK:
        if (is_broadcast)
        {
          ret = MAC_TX_OK;
        }
        else
        {
          /* Check for ack */

          /* Wait for max CSMA_ACK_WAIT_TIME */
          // RTIMER_BUSYWAIT_UNTIL(NETSTACK_RADIO.pending_packet, CSMA_ACK_WAIT_TIME);

          uint32_t time = 0;
          while ((time < CSMA_ACK_WAIT_TIME) || !(NETSTACK_RADIO.pending_packet))
          {
            watchdog_periodic();
            clock_delay_usec(500);
            time++;
          }

          ret = MAC_TX_NOACK;

          if (NETSTACK_RADIO.receiving_packet() ||
              NETSTACK_RADIO.pending_packet() ||
              NETSTACK_RADIO.channel_clear() == 0)
          {
            int len;
            uint8_t ackbuf[ACK_LEN];

            // the following part is new in contiki-ng
            /* Wait an additional CSMA_AFTER_ACK_DETECTED_WAIT_TIME to complete reception */
            RTIMER_BUSYWAIT_UNTIL(NETSTACK_RADIO.pending_packet(), CSMA_AFTER_ACK_DETECTED_WAIT_TIME);

            if (NETSTACK_RADIO.pending_packet())
            {
              len = NETSTACK_RADIO.read(ackbuf, ACK_LEN);
              if (len == ACK_LEN && ackbuf[2] == dsn)
              {
                /* Ack received */
                LOG_INFO("ACK received\n");
                ret = MAC_TX_OK;
              }
              else
              {
                /* Not an ack or ack not for us: collision */
                LOG_INFO("NO ACK or not for us\n");
                ret = MAC_TX_COLLISION;
              }
            }
          }
        }
        break;
      case RADIO_TX_COLLISION:
        ret = MAC_TX_COLLISION;
        break;
      default:
        ret = MAC_TX_ERR;
        break;
      }
    }
  }
  if (ret == MAC_TX_OK)
  {
    last_sent_ok = 1;
  }
  mac_call_sent_callback(sent, ptr, ret, 1);
  return last_sent_ok;
}
/*---------------------------------------------------------------------------*/
static void
send_packet(mac_callback_t sent, void *ptr)
{
  LOG_DBG("Function call: %s\n", __func__);
  send_one_packet(sent, ptr);
}
/*---------------------------------------------------------------------------*/
static void
send_list(mac_callback_t sent, void *ptr, struct packet_queue *q)
{
  LOG_DBG("Function call: %s\n", __func__);
  while (q != NULL)
  {
    /* We backup the next pointer, as it may be nullified by
     * mac_call_sent_callback() */
    struct packet_queue *next = q->next;

    queuebuf_to_packetbuf(q->buf);
    int last_sent_ok;
    last_sent_ok = send_one_packet(sent, ptr);

    /* If packet transmission was not successful, we should back off and let
     * upper layers retransmit, rather than potentially sending out-of-order
     * packet fragments. */
    if (!last_sent_ok)
    {
      return;
    }
    q = next;
  }
}
/*---------------------------------------------------------------------------*/
static void
packet_input(void)
{
  LOG_DBG("Function call: %s\n", __func__);
  NETSTACK_MAC.input();
  packet_received = true;
}
/*---------------------------------------------------------------------------*/
static int
on(void)
{
  LOG_DBG("Function call: %s\n", __func__);

  NETSTACK_RADIO.on();

  return 1;
}
/*---------------------------------------------------------------------------*/
static int
off(void)
{
  LOG_DBG("Function call: %s\n", __func__);
  LOG_DBG("\n\n\n\n");

  radio_on = false;
  return NETSTACK_RADIO.off();
}
/*---------------------------------------------------------------------------*/
static unsigned short
channel_check_interval(void)
{
  LOG_DBG("Function call: %s\n", __func__);
  return 0;
}
/*---------------------------------------------------------------------------*/
const struct rdc_driver nullrdc_driver = {
    "nullrdc",
    init,
    send_packet,
    send_list,
    packet_input,
    on,
    off,
    channel_check_interval,
};
/*---------------------------------------------------------------------------*/
