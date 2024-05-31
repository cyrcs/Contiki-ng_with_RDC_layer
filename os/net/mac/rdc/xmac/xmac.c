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
 *         An XMAC implementation that uses framer for headers.
 * \author
 *         Adam Dunkels <adam@sics.se>
 *         Niclas Finne <nfi@sics.se>
 */

#include "net/mac/rdc/xmac/xmac.h"
#include "net/packetbuf.h"
#include "net/queuebuf.h"
#include "net/netstack.h"
#include <string.h>
#include "dev/leds.h"

// TODO find a better way to include to required functions/constants
#include "net/mac/csma_with_rdc/csma-security.h"
#include "net/mac/csma_with_rdc/csma.h"

#include "dev/radio.h"
#include "sys/compower.h"
#include "dev/watchdog.h"
#include "sys/ctimer.h"
#include "sys/rtimer.h"

#include "sys/clock.h"
#include "lib/random.h"
#include "lib/list.h"
#include "lib/memb.h"
#include "lib/assert.h"

#include "sys/pt.h"

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "XMAC"
#define LOG_LEVEL LOG_LEVEL_RDC

#define ACK_LEN 3

// LEDS
#if 1
#undef LEDS_ON
#undef LEDS_OFF
#undef LEDS_TOGGLE
#define DEBUG_LEDS 1
#include <stdio.h>
#include <math.h>
#if DEBUG_LEDS
#define LEDS_ON(x) leds_on(x)
#define LEDS_OFF(x) leds_off(x)
#define LEDS_TOGGLE(x) leds_toggle(x)
#else
#define LEDS_ON(x)
#define LEDS_OFF(x)
#define LEDS_TOGGLE(x)
#endif
#endif

// Timings
#if 1
#ifdef XMAC_CONF_ON_TIME
#define DEFAULT_ON_TIME (XMAC_CONF_ON_TIME)
#else
#define DEFAULT_ON_TIME (7 * RTIMER_ARCH_SECOND / 2)
#endif // ToA depends on the SF and BW config but can vary from < 1ms to 5s

#ifdef XMAC_CONF_OFF_TIME
#define DEFAULT_OFF_TIME (XMAC_CONF_OFF_TIME)
#else
// #define DEFAULT_OFF_TIME (CLOCK_SECOND / NETSTACK_RDC_CHANNEL_CHECK_RATE - DEFAULT_ON_TIME)
#define DEFAULT_OFF_TIME (20 * RTIMER_ARCH_SECOND)

#define DEFAULT_DATA_WAIT_TIME (6 * RTIMER_ARCH_SECOND)

#endif

#define DEFAULT_PERIOD (DEFAULT_OFF_TIME + DEFAULT_ON_TIME)

#define DEFAULT_STROBE_PERIOD DEFAULT_PERIOD + DEFAULT_ON_TIME

// check if period is not 0 since this will cause issues
#if DEFAULT_PERIOD == 0
#undef DEFAULT_PERIOD
#define DEFAULT_PERIOD 1
#endif

#define DEFAULT_STROBE_WAIT_TIME (1.1 * RTIMER_ARCH_SECOND)
#endif

#define DISPATCH 0
#define TYPE_STROBE 0x10
// #define TYPE_DATA 0x11
#define TYPE_ANNOUNCEMENT 0x12
#define TYPE_STROBE_ACK 0x13

#define MAX_STROBE_SIZE 50
#define WAIT_TIME_BEFORE_STROBE_ACK CLOCK_SECOND

static volatile uint8_t xmac_is_on = 0;

static volatile unsigned char waiting_for_packet = 0;
static volatile unsigned char someone_is_sending = 0;
static volatile unsigned char we_are_sending = 0;
static volatile unsigned char radio_is_on = 0;

static struct pt pt;

struct cxmac_config cxmac_config = {
    DEFAULT_ON_TIME,
    DEFAULT_OFF_TIME,
    DEFAULT_STROBE_PERIOD,
    DEFAULT_STROBE_WAIT_TIME};

struct cxmac_hdr
{
  uint8_t dispatch;
  uint8_t type;
};
/*---------------------------------------------------------------------------*/
static struct ctimer cpowercycle_ctimer;
#define CSCHEDULE_POWERCYCLE(rtime) cschedule_powercycle((1ul * CLOCK_SECOND * (rtime)) / RTIMER_ARCH_SECOND)
static char cpowercycle(void *ptr);

static void cschedule_powercycle(clock_time_t time)
{
  LOG_FUNC("Function call: %s\n", __func__);

  if (xmac_is_on)
  {
    if (time == 0)
    {
      time = 1;
    }
    ctimer_set(&cpowercycle_ctimer, time,
               (void (*)(void *))cpowercycle, NULL);
  }
}

/*---------------------------------------------------------------------------*/
static int on(void)
{
  LOG_FUNC("Function call: %s\n", __func__);

  xmac_is_on = 1;
  CSCHEDULE_POWERCYCLE(DEFAULT_OFF_TIME);

  return 1;
}
static void turn_radio_on(void)
{
  LOG_FUNC("Function call: %s\n", __func__);

  if (xmac_is_on && !radio_is_on)
  {
    LOG_INFO("Turning radio on\n");
    radio_is_on = 1;
    NETSTACK_RADIO.on();
    LEDS_ON(LEDS_GREEN);
  }
}
static void powercycle_turn_radio_on(void)
{
  LOG_FUNC("Function call: %s\n", __func__);

  if (we_are_sending == 0 &&
      waiting_for_packet == 0)
  {
    turn_radio_on();
  }
}
/*---------------------------------------------------------------------------*/
static int off(void)
{
  LOG_FUNC("Function call: %s\n", __func__);

  xmac_is_on = 0;

  return 1;
}
static void turn_radio_off(void)
{
  LOG_FUNC("Function call: %s\n", __func__);

  if (radio_is_on)
  {
    LOG_INFO("Turning radio off\n");
    radio_is_on = 0;
    NETSTACK_RADIO.off();
    LEDS_OFF(LEDS_GREEN);
  }
}
static void powercycle_turn_radio_off(void)
{
  LOG_FUNC("Function call: %s\n", __func__);

  if (we_are_sending == 0 &&
      waiting_for_packet == 0)
  {
    turn_radio_off();
  }
}

/*---------------------------------------------------------------------------*/ static char cpowercycle(void *ptr)
{
  LOG_FUNC("Function call: %s\n", __func__);

  PT_BEGIN(&pt);

  while (1)
  {
    /* Only wait for some cycles to pass for someone to start sending */
    if (someone_is_sending > 0)
    {
      someone_is_sending--;
    }

    /* If there were a strobe in the air, turn radio on */
    powercycle_turn_radio_on();
    CSCHEDULE_POWERCYCLE(DEFAULT_ON_TIME);
    PT_YIELD(&pt);

    if (cxmac_config.off_time > 0)
    {
      powercycle_turn_radio_off();
      if (waiting_for_packet != 0)
      {
        waiting_for_packet++;
        if (waiting_for_packet > 2)
        {
          /* We should not be awake for more than two consecutive
             power cycles without having heard a packet, so we turn off
             the radio. */
          waiting_for_packet = 0;
          powercycle_turn_radio_off();
        }
      }
      CSCHEDULE_POWERCYCLE(DEFAULT_OFF_TIME);
      PT_YIELD(&pt);
    }
  }

  PT_END(&pt);
}
/*---------------------------------------------------------------------------*/

static int
send_one_packet()
{
  LOG_FUNC("Function call: %s\n", __func__);

  // variables
  rtimer_clock_t t0;
  rtimer_clock_t t;
  // rtimer_clock_t encounter_time = 0;
  int strobes;
  struct cxmac_hdr *hdr;
  int got_strobe_ack = 0;
  uint8_t strobe[MAX_STROBE_SIZE];
  int strobe_len, len;
  int is_broadcast = 0;
  int is_dispatch, is_strobe_ack;
  /*int is_reliable;*/
  // struct encounter *e;
  struct queuebuf *packet;
  int is_already_streaming = 0;
  uint8_t collisions;

  LEDS_ON(LEDS_BLUE);

  /* Create the X-MAC header for the data packet. */
#if !NETSTACK_CONF_BRIDGE_MODE
  /* If NETSTACK_CONF_BRIDGE_MODE is set, assume PACKETBUF_ADDR_SENDER is already set. */
  packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &linkaddr_node_addr);
  packetbuf_set_attr(PACKETBUF_ATTR_MAC_ACK, 1);

#endif
  if (packetbuf_holds_broadcast())
  {
    is_broadcast = 1;
    LOG_DBG("xmac: send broadcast\n");
  }
  else
  {
    LOG_DBG("xmac: send unicast to %02x%02x:%02x%02x:%02x%02x:%02x%02x\n",
            packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[0],
            packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[1],
            packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[2],
            packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[3],
            packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[4],
            packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[5],
            packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[6],
            packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[7]);
  }

  // len = NETSTACK_FRAMER.create();
  len = csma_security_create_frame();
  strobe_len = len + sizeof(struct cxmac_hdr);
  if (len < 0 || strobe_len > (int)sizeof(strobe))
  {
    /* Failed to send */
    LOG_INFO("send failed, too large header\n");
    return MAC_TX_ERR_FATAL;
  }
  memcpy(strobe, packetbuf_hdrptr(), len);
  strobe[len] = DISPATCH;        /* dispatch */
  strobe[len + 1] = TYPE_STROBE; /* type */

  packetbuf_compact();
  packet = queuebuf_new_from_packetbuf();
  if (packet == NULL)
  {
    /* No buffer available */
    LOG_WARN("send failed, no queue buffer available (of %u)\n",
             QUEUEBUF_CONF_NUM);
    return MAC_TX_ERR;
  }

  turn_radio_off();

  /* By setting we_are_sending to one, we ensure that the rtimer
     powercycle interrupt do not interfere with us sending the packet. */
  we_are_sending = 1;

  t0 = RTIMER_NOW();
  strobes = 0;

  /* Send a train of strobes until the receiver answers with an ACK. */
  turn_radio_on();
  collisions = 0;
  if (!is_already_streaming)
  {
    // watchdog_stop();
    watchdog_periodic();
    got_strobe_ack = 0;
    for (strobes = 0, collisions = 0;
         got_strobe_ack == 0 && collisions == 0 &&
         RTIMER_CLOCK_LT(RTIMER_NOW(), t0 + cxmac_config.strobe_time);
         strobes++)
    {

      /* Send the strobe packet. */
      if (got_strobe_ack == 0 && collisions == 0)
      {
        LOG_INFO("sending strobe\n");
        if (is_broadcast)
        {
#if WITH_STROBE_BROADCAST
          NETSTACK_RADIO.send(strobe, strobe_len);
#else
          queuebuf_to_packetbuf(packet);
          NETSTACK_RADIO.send(packetbuf_hdrptr(), packetbuf_totlen());
#endif
          turn_radio_off();
        }
        else
        {
          NETSTACK_RADIO.send(strobe, strobe_len);
        }
      }

      // watchdog_periodic();
      t = RTIMER_NOW();

      while (got_strobe_ack == 0 &&
             RTIMER_CLOCK_LT(RTIMER_NOW(), t + cxmac_config.strobe_wait_time))
      {
        /* If the device keeps on resetting due to the watchdog timer suspecting a lock-up, uncomment the following line */
        // watchdog_periodic();

        // If we have a pending packet, check if it is an ACK for the strobe
        packetbuf_clear();
        len = NETSTACK_RADIO.read(packetbuf_dataptr(), PACKETBUF_SIZE);
        if (len > 0)
        {
          LOG_DBG("received packet (%d) => anaylzing\n", len);
          packetbuf_set_datalen(len);
          if (csma_security_parse_frame() >= 0)
          {
            hdr = packetbuf_dataptr();
            is_dispatch = hdr->dispatch == DISPATCH;
            is_strobe_ack = hdr->type == TYPE_STROBE_ACK;
            LOG_DBG("dispatch: %d, type: %d\n", is_dispatch, is_strobe_ack);
            if (is_dispatch && is_strobe_ack)
            {
              if (linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
                               &linkaddr_node_addr))
              {
                LOG_DBG("received strobe ack\n");
                got_strobe_ack = 1;
                /* We got an ACK from the receiver, so we can immediately send
                   the packet. */
              }
              else
              {
                LOG_DBG("received strobe ack not for us\n");
              }
            }
            else
            {
              LOG_INFO("send failed to parse %u\n", len);
              collisions++;
            }
          }
          else
          {
            LOG_DBG("failed to parse frame\n");
          }
        }
      }
    }
  }

  packetbuf_clear();
  // packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &linkaddr_node_addr);

  /* restore the packet to send */
  queuebuf_to_packetbuf(packet);
  queuebuf_free(packet);

  /* Send the data packet. */
  uint8_t dsn = 0;
  int ret;
  if ((is_broadcast || got_strobe_ack) && collisions == 0)
  {
    LOG_DBG("Sending data packet\n");
    dsn = ((uint8_t *)packetbuf_hdrptr())[2] & 0xff;
    NETSTACK_RADIO.send(packetbuf_hdrptr(), packetbuf_totlen());
    // turn radio on to receive ACK
    // turn_radio_on();
  }

  LOG_INFO("send (strobes=%u,len=%u,%s), done\n", strobes - got_strobe_ack,
           packetbuf_totlen(), got_strobe_ack ? "ack" : "no ack");

  LEDS_OFF(LEDS_BLUE);

  if (collisions == 0)
  {
    if (!is_broadcast && !got_strobe_ack)
    {
      we_are_sending = 0;

      ret = MAC_TX_NOACK;
    }
    else if (is_broadcast)
    {
      we_are_sending = 0;
      ret = MAC_TX_OK;
    }
    else
    {
      RTIMER_BUSYWAIT_UNTIL(NETSTACK_RADIO.pending_packet(), CSMA_ACK_WAIT_TIME);
      ret = MAC_TX_NOACK;

      if (NETSTACK_RADIO.receiving_packet() ||
          NETSTACK_RADIO.pending_packet() ||
          NETSTACK_RADIO.channel_clear() == 0)
      {
        int len;
        uint8_t ackbuf[ACK_LEN];

        LOG_INFO("Waiting for data packet ACK\n");
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
  }
  else
  {
    someone_is_sending++;
    ret = MAC_TX_COLLISION;
  }

  // return
  we_are_sending = 0;
  return ret;
}
/*---------------------------------------------------------------------------*/
static void
send_packet(mac_callback_t sent, void *ptr)
{
  LOG_FUNC("Function call: %s\n", __func__);

  int ret;
  if (someone_is_sending)
  {
    LOG_INFO("should queue packet, now just dropping %d %d %d %d.\n",
             waiting_for_packet, someone_is_sending, we_are_sending, radio_is_on);
    // RIMESTATS_ADD(sendingdrop);
    ret = MAC_TX_COLLISION;
  }
  else
  {
    LOG_INFO("send immediately.\n");
    ret = send_one_packet();
  }

  mac_call_sent_callback(sent, ptr, ret, 1);
}
/*---------------------------------------------------------------------------*/
static void
send_list(mac_callback_t sent, void *ptr, struct packet_queue *q)
{
  LOG_FUNC("Function call: %s\n", __func__);

  if (q != NULL)
  {
    queuebuf_to_packetbuf(q->buf);
    send_packet(sent, ptr);
  }
}
/*---------------------------------------------------------------------------*/
static void
packet_input(void)
{
  LOG_FUNC("Function call: %s\n", __func__);
  struct cxmac_hdr *hdr;

  LEDS_ON(LEDS_RED);

  // once a packet is received, the radio should go to sleep
  // turn_radio_off();

  if (csma_security_parse_frame() < 0)
  {
    LOG_ERR("failed to parse (%u)\n", packetbuf_datalen());
  }

  hdr = packetbuf_dataptr();

  LOG_INFO("received packet: (%u)\n", packetbuf_datalen());

  if (hdr->dispatch != DISPATCH)
  {
    someone_is_sending = 0;
    if (linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
                     &linkaddr_node_addr) ||
        linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
                     &linkaddr_null))
    {
      /* This is a regular packet that is destined to us or to the
         broadcast address. */

#if CSMA_SEND_SOFT_ACK
      uint8_t ackdata[CSMA_ACK_LEN];
      if (packetbuf_attr(PACKETBUF_ATTR_MAC_ACK))
      {
        ackdata[0] = FRAME802154_ACKFRAME;
        ackdata[1] = 0;
        ackdata[2] = ((uint8_t *)packetbuf_hdrptr())[2];
        LOG_DBG("SENDING ACK\n");
        NETSTACK_RADIO.send(ackdata, CSMA_ACK_LEN);
      }
      LOG_DBG("DONE SENDING ACK\n");

      // if ACK is send, turn off radio again
      turn_radio_off();
#endif /* CSMA_SEND_SOFT_ACK */

      // waiting_for_packet = 0;

      LOG_DBG("data(%u)\n", packetbuf_datalen());
      NETSTACK_MAC.input();

      // once packet is received, force to off state
      // on();

      LEDS_OFF(LEDS_RED);

      return;
    }
    else
    {
      LOG_DBG("data not for us\n");
    }
  }
  else if (hdr->type == TYPE_STROBE)
  {
    LOG_DBG("header type: strobe\n");
    someone_is_sending = 2;

    if (linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
                     &linkaddr_node_addr))
    {
      LOG_INFO("Received strobe for us\n");
      // packetbuf_clear();

      /* This is a strobe packet for us. */

      /* If the sender address is someone else, we should
         acknowledge the strobe and wait for the packet. By using
         the same address as both sender and receiver, we flag the
         message is a strobe ack. */
      hdr->type = TYPE_STROBE_ACK;
      packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER,
                         packetbuf_addr(PACKETBUF_ADDR_SENDER));
      packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &linkaddr_node_addr);
      packetbuf_compact();
      if (csma_security_create_frame() >= 0)
      {
        /* We turn on the radio in anticipation of the incoming
           packet. */
        someone_is_sending = 1;
        waiting_for_packet = 1;
        NETSTACK_RADIO.send(packetbuf_hdrptr(), packetbuf_totlen());
        LOG_INFO("send strobe ack (%u)\n", packetbuf_totlen());
        // if strobe ACK is send, we expect a data packet so the radio should be on
        turn_radio_on();
      }
      else
      {
        LOG_INFO("failed to send strobe ack\n");
      }
    }
    else if (linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
                          &linkaddr_null))
    {
      /* If the receiver address is null, the strobe is sent to
         prepare for an incoming broadcast packet. If this is the
         case, we turn on the radio and wait for the incoming
         broadcast packet. */
      waiting_for_packet = 1;
      turn_radio_on();
    }
    else
    {
      LOG_DBG("strobe not for us\n");
    }

    /* We are done processing the strobe and we therefore return
 to the caller. */
    LEDS_OFF(LEDS_RED);

    return;
  }
  else if (hdr->type == TYPE_STROBE_ACK)
  {
    LOG_DBG("header type: strobe ack\n");
  }
  else
  {
    LOG_INFO("unknown type %u (%u): %s\n", hdr->type,
             packetbuf_datalen(), (char *)packetbuf_dataptr());
  }
  LEDS_OFF(LEDS_RED);
}

/*---------------------------------------------------------------------------*/
static unsigned short
channel_check_interval(void)
{
  LOG_FUNC("Function call: %s\n", __func__);
  // printf("%lu\n", (1ul * CLOCK_SECOND * DEFAULT_PERIOD) / RTIMER_ARCH_SECOND);
  return 1;
}
/*---------------------------------------------------------------------------*/
static void
init(void)
{
  LOG_FUNC("Function call: %s\n", __func__);

  LOG_INFO("Initializing XMAC with ON: %lu ms, OFF: %lu ms, STROBE: %lu ms, STROBE_WAIT: %lu ms\n",
           (unsigned long)cxmac_config.on_time * 1000 / RTIMER_ARCH_SECOND,
           (unsigned long)cxmac_config.off_time * 1000 / RTIMER_ARCH_SECOND,
           (unsigned long)cxmac_config.strobe_time * 1000 / RTIMER_ARCH_SECOND,
           (unsigned long)cxmac_config.strobe_wait_time * 1000 / RTIMER_ARCH_SECOND);

  radio_is_on = 0;
  waiting_for_packet = 0;
  PT_INIT(&pt);

  xmac_is_on = 1;

  CSCHEDULE_POWERCYCLE(DEFAULT_OFF_TIME);
}
/*---------------------------------------------------------------------------*/
const struct rdc_driver xmac_driver = {
    "xmac",
    init,
    send_packet,
    send_list,
    packet_input,
    on,
    off,
    channel_check_interval,
};
/*---------------------------------------------------------------------------*/
