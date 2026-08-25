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
// LEDS
#if 1
#undef LEDS_ON
#undef LEDS_OFF
#undef LEDS_TOGGLE
#ifdef XMAC_CONF_DEBUG_LEDS
#define DEBUG_LEDS XMAC_CONF_DEBUG_LEDS
#else
#define DEBUG_LEDS 1
#endif
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

#define DEFAULT_STROBE_PERIOD 2 * DEFAULT_PERIOD + DEFAULT_ON_TIME

// check if period is not 0 since this will cause issues
#if DEFAULT_PERIOD == 0
#undef DEFAULT_PERIOD
#define DEFAULT_PERIOD 1
#endif

#define DEFAULT_STROBE_WAIT_TIME (1.1 * RTIMER_ARCH_SECOND)
#endif

#ifndef XMAC_CONF_USE_CACHE
#define XMAC_CONF_USE_CACHE 1   /* 1 = cache-mechanisme actief, 0 = altijd cache-miss */
#endif

#define DISPATCH 0
#define TYPE_STROBE 0x10
#define TYPE_DATA 0x11
#define TYPE_ANNOUNCEMENT 0x12
#define TYPE_STROBE_ACK 0x13

#define MAX_STROBE_SIZE 50
#define WAIT_TIME_BEFORE_STROBE_ACK CLOCK_SECOND

#define SEEN_FRAMES_SIZE 10

#define MAX_ENCOUNTERS 4

#define CACHE_HIT_WAKEUP_DELAY ((rtimer_clock_t)( 0.1 * RTIMER_SECOND))


static volatile uint8_t xmac_is_on = 0;

static volatile unsigned char waiting_for_packet = 0;
static volatile unsigned char someone_is_sending = 0;
static volatile unsigned char we_are_sending = 0;
static volatile unsigned char radio_is_on = 0;
static uint16_t frame_counter = 0;
static uint8_t seen_frames_index = 0;

static struct pt pt;

struct seen_frame {
  uint16_t signature;
};
static struct seen_frame seen_frames[SEEN_FRAMES_SIZE];

struct encounter {
  struct encounter *next;
  linkaddr_t neighbor; 
  rtimer_clock_t time;
};



LIST(encounter_list);
MEMB(encounter_memb, struct encounter, MAX_ENCOUNTERS);

struct cxmac_config cxmac_config = {
    DEFAULT_ON_TIME,
    DEFAULT_OFF_TIME,
    DEFAULT_STROBE_PERIOD,
    DEFAULT_STROBE_WAIT_TIME};

struct cxmac_hdr
{
  uint8_t dispatch;
  uint8_t type;
  uint16_t frame_id;
  uint8_t checksum;
};

static uint32_t stat_tx_attempts = 0;
static uint32_t stat_tx_ok = 0;
static uint32_t stat_tx_noack = 0;
static uint32_t stat_tx_collision = 0;
static uint32_t stat_tx_err = 0;

static uint32_t stat_rx_delivered = 0;
static uint32_t stat_rx_mismatch = 0;

/* NIEUW: vervangt expect_duplicate_data / duplicate_data_sender.
   Bijgehouden bij ontvangst van een geldige strobe, gebruikt om het
   bijhorende databericht te valideren (mismatch = corrupt/verouderd). */
static uint16_t expected_data_frame_id;
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
/*ZELF TOEGEVOEGD*/
static uint16_t compute_frame_signature(uint16_t frame_id)
{
  uint16_t sender_id = (packetbuf_addr(PACKETBUF_ADDR_SENDER)->u8[6] << 8) |
                        packetbuf_addr(PACKETBUF_ADDR_SENDER)->u8[7];
  uint16_t receiver_id = (packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[6] << 8) |
                          packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[7];
  return sender_id + receiver_id + frame_id;
}

static uint8_t is_duplicate_frame(uint16_t frame_id)
{
  for(uint8_t i = 0; i < SEEN_FRAMES_SIZE; i++){
    if(seen_frames[i].signature == frame_id){
      LOG_DBG("Frame_id bestaat al -> dropped");
      return 1;
    }
  }
  seen_frames[seen_frames_index].signature = frame_id;
  seen_frames_index = (seen_frames_index + 1) % SEEN_FRAMES_SIZE;
  return 0;
}

static uint8_t compute_checksum(struct cxmac_hdr *hdr)
{
  return hdr->dispatch ^
         hdr -> type ^
         (hdr->frame_id >> 8) ^
         (hdr->frame_id & 0xFF);
}
#if XMAC_CONF_USE_CACHE
static void register_encounter(const linkaddr_t *neighbor, rtimer_clock_t time){
  struct encounter *e;

  for(e = list_head(encounter_list); e != NULL; e = list_item_next(e)){
    if(linkaddr_cmp(neighbor, &e->neighbor)){
      e->time = time;
      break;
    }
  }
  if(e == NULL){
    e = memb_alloc(&encounter_memb);
    if(e == NULL){
      return;
    }
    linkaddr_copy(&e->neighbor, neighbor);
    e->time = time;
    list_add(encounter_list, e);
  }
}
static void forget_encounter(const linkaddr_t *neighbor)
{
  struct encounter *e;

  for(e = list_head(encounter_list); e != NULL; e = list_item_next(e)) {
    if(linkaddr_cmp(neighbor, &e->neighbor)) {
      list_remove(encounter_list, e);
      memb_free(&encounter_memb, e);
      LOG_INFO("Encounter cache entry gewist voor %02x:%02x (geen ack na cache-hit strobe-trein)\n",
               neighbor->u8[6], neighbor->u8[7]);
      break;
    }
  }
}
#endif
/*--------------------------------------------------------------------------------*/
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
//Oude functie send_one_packet is in document oude_code
/*---------------------------------------------------------------------------*/
static int
send_one_packet()
{
  LOG_FUNC("Function call: %s\n", __func__);

  // variables
  rtimer_clock_t t0;
  rtimer_clock_t t;
  rtimer_clock_t encounter_time = 0;
  int strobes;
  struct cxmac_hdr *hdr;
  uint16_t data_frame_id;   /* frame_id apart bewaren, want 'hdr' wordt later hergebruikt */
  int got_strobe_ack = 0;
  uint8_t strobe[MAX_STROBE_SIZE];
  int strobe_len, len;
  int csma_hdr_len;   /* aparte, stabiele kopie van de CSMA-headerlengte,
                          want 'len' wordt verderop hergebruikt in de backoff-
                          en strobe-wait-lussen en is dus niet meer betrouwbaar
                          tegen de tijd dat de data verstuurd wordt */
  int is_broadcast = 0;
  int is_dispatch, is_strobe_ack;
  /*int is_reliable;*/
  #if XMAC_CONF_USE_CACHE
  struct encounter *e;
  #endif

  struct queuebuf *packet;
  int is_already_streaming = 0;
  uint8_t collisions;

  LEDS_ON(LEDS_BLUE);

  stat_tx_attempts++;   /* NIEUW: elke aanroep van send_one_packet() telt als 1 poging */

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
  csma_hdr_len = len;   /* bewaren vóór 'len' verderop hergebruikt wordt */
  strobe_len = len + sizeof(struct cxmac_hdr);
  if (len < 0 || strobe_len > (int)sizeof(strobe))
  {
    /* Failed to send */
    stat_tx_err++;   /* NIEUW */
    LOG_INFO("send failed, too large header\n");
    return MAC_TX_ERR_FATAL;
  }
  memcpy(strobe, packetbuf_hdrptr(), len);
  strobe[len] = DISPATCH;        /* dispatch */
  strobe[len + 1] = TYPE_STROBE; /* type */
  hdr = (struct cxmac_hdr *)&strobe[len];
  hdr->frame_id = compute_frame_signature(frame_counter++); /*frame_id*/
  hdr->checksum = compute_checksum(hdr);
  data_frame_id = hdr->frame_id; /* bewaren zolang 'hdr' nog naar de strobe wijst */

  //uint8_t *fid = (uint8_t *)&hdr->frame_id;
  //fid[0] ^= 0x01;  /* bit 0 van frame_id high-byte: 0 -> 1 */
  //fid[1] ^= 0x01;  /* bit 0 van frame_id low-byte: 0 -> 1 */
  LOG_INFO("TX: frame_id = %u naar%02x:%02x\n", hdr->frame_id, packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[6],
           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[7]);

  packetbuf_compact();
  packet = queuebuf_new_from_packetbuf();
  if (packet == NULL)
  {
    /* No buffer available */
    stat_tx_err++;   /* NIEUW */
    LOG_WARN("send failed, no queue buffer available (of %u)\n",
             QUEUEBUF_CONF_NUM);
    return MAC_TX_ERR;
  }

  turn_radio_off();

  uint8_t cache_hit = 0;
#if XMAC_CONF_USE_CACHE
  for(e = list_head(encounter_list); e != NULL; e = list_item_next(e)) {
    const linkaddr_t *receiver = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);

    if(linkaddr_cmp(receiver, &e->neighbor)) {
      rtimer_clock_t wait, now, expected, elapsed, phase;

      now = RTIMER_NOW();

      elapsed = now - e->time;
      phase = elapsed % DEFAULT_PERIOD;
      wait = (phase == 0) ? 0 : (DEFAULT_PERIOD - phase);

      expected = now + wait + CACHE_HIT_WAKEUP_DELAY;

      LOG_INFO("CACHE-HIT voor buur, wacht tot verwacht wake-up (wait=%lu)\n",(unsigned long)wait);
      while(RTIMER_CLOCK_LT(RTIMER_NOW(), expected)){
        watchdog_periodic();
      }
      cache_hit = 1;
      break;
    }
  }
#endif /* XMAC_CONF_USE_CACHE */
  if(!cache_hit){
    LOG_INFO("CACHE-MIS, geen encounter bekend, volledige strobe-trein\n");
  }
  we_are_sending = 1;

  turn_radio_on();

  if (!cache_hit)
  {
    rtimer_clock_t backoff = (random_rand() % (DEFAULT_ON_TIME));
    rtimer_clock_t backoff_start = RTIMER_NOW();
    uint8_t someone_else_strobing = 0;

    while (RTIMER_CLOCK_LT(RTIMER_NOW(), backoff_start + backoff))
    {
      watchdog_periodic();
      packetbuf_clear();
      len = NETSTACK_RADIO.read(packetbuf_dataptr(), PACKETBUF_SIZE);
      if (len > 0)
      {
        packetbuf_set_datalen(len);
        if (csma_security_parse_frame() >= 0 &&
            len >= (int)sizeof(struct cxmac_hdr))
        {
          struct cxmac_hdr *probe_hdr = packetbuf_dataptr();
          if (probe_hdr->dispatch == DISPATCH && probe_hdr->type == TYPE_STROBE)
          {
            LOG_INFO("Andere node is al aan het strobe'n, wij wachten\n");
            someone_else_strobing = 1;
            break;
          }
        }
      }
    }

    if (someone_else_strobing)
    {
      turn_radio_off();
      we_are_sending = 0;
      queuebuf_free(packet);
      stat_tx_collision++;   /* NIEUW */
      return MAC_TX_COLLISION;
    }
  }

  t0 = RTIMER_NOW();
  strobes = 0;

  /* Send a train of strobes until the receiver answers with an ACK. */
  collisions = 0;
  if (!is_already_streaming)
  {
    got_strobe_ack = 0;
    for (strobes = 0, collisions = 0;
         got_strobe_ack == 0 && collisions == 0 &&
         RTIMER_CLOCK_LT(RTIMER_NOW(), t0 + cxmac_config.strobe_time);
         strobes++)
    {
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

      t = RTIMER_NOW();

      while (got_strobe_ack == 0 && RTIMER_CLOCK_LT(RTIMER_NOW(), t + cxmac_config.strobe_wait_time)) {
        watchdog_periodic();

        packetbuf_clear();
        len = NETSTACK_RADIO.read(packetbuf_dataptr(), PACKETBUF_SIZE);
        if (len > 0)
        {
          LOG_DBG("received packet (%d) => anaylzing\n", len);
          packetbuf_set_datalen(len);
          if (csma_security_parse_frame() >= 0) {

            if(len < (int)sizeof(struct cxmac_hdr)) {
              LOG_DBG("Genegeerd: Frame te kort voor cxmac header (%d bytes)\n", len);
            }
            else {

              hdr = packetbuf_dataptr();  /* hdr wijst hierna NIET meer naar de strobe-header */
              is_dispatch = hdr->dispatch == DISPATCH;
              is_strobe_ack = hdr->type == TYPE_STROBE_ACK;
              LOG_DBG("dispatch: %d, type: %d\n", is_dispatch, is_strobe_ack);
              if (is_dispatch && is_strobe_ack) {

                if (linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER), &linkaddr_node_addr)) {

                  LOG_DBG("received strobe ack\n");
                  got_strobe_ack = 1;
                  encounter_time = RTIMER_NOW();
                  LOG_INFO("Strobe-ack ontvangen, encounter_time=%lu\n", (unsigned long)encounter_time);
                }
                else
                {
                  LOG_DBG("received strobe ack not for us\n");
                }
              }
              else {

                LOG_INFO("send failed to parse %u\n", len);
                collisions++;
              }
            }
          }
          else {

            LOG_DBG("failed to parse frame\n");
          }
        }
      }
    }
  }

#if XMAC_CONF_USE_CACHE
  if (cache_hit && !got_strobe_ack)
  {
    forget_encounter(packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
  }
#endif

  packetbuf_clear();

  /* restore the packet to send */
  queuebuf_to_packetbuf(packet);
  queuebuf_free(packet);

  /* Send the data packet. */
  uint8_t dsn = 0;
  int ret;
  if ((is_broadcast || got_strobe_ack) && collisions == 0)
  {
    LOG_DBG("Sending data packet\n");

    /* dsn uit de CSMA/802.15.4-header lezen — byte [2] = sequence number,
       ongewijzigd t.o.v. voorheen. */
    dsn = ((uint8_t *)packetbuf_hdrptr())[2] & 0xff;

    /* cxmac_hdr manueel invoegen TUSSEN de CSMA-header en de
       payload, net zoals bij de strobe. packetbuf_hdralloc() zet nieuwe
       bytes vóór de HELE bestaande header i.p.v. ertussen, waardoor de
       ontvanger de dispatch-byte verkeerd las ("geen geldige dispatch
       byte") — vandaar deze manuele opbouw i.p.v. hdralloc. */
    {
      static uint8_t data_frame[PACKETBUF_SIZE + sizeof(struct cxmac_hdr)];
      int payload_len = packetbuf_totlen() - csma_hdr_len;
      int total_len = csma_hdr_len + (int)sizeof(struct cxmac_hdr) + payload_len;

      if (payload_len < 0 || total_len > (int)sizeof(data_frame))
      {
        LOG_WARN("send failed, data + header te groot (%d)\n", total_len);
        we_are_sending = 0;
        stat_tx_err++;   /* NIEUW */
        return MAC_TX_ERR;
      }

      /* CSMA/802.15.4-header, ongewijzigd, vooraan */
      memcpy(data_frame, packetbuf_hdrptr(), csma_hdr_len);

      /* cxmac_hdr, net na de CSMA-header */
      struct cxmac_hdr *data_hdr = (struct cxmac_hdr *)&data_frame[csma_hdr_len];
      data_hdr->dispatch = DISPATCH;
      data_hdr->type = TYPE_DATA;
      data_hdr->frame_id = data_frame_id;
      data_hdr->checksum = compute_checksum(data_hdr);

      /* applicatie-payload, na de cxmac_hdr */
      memcpy(data_frame + csma_hdr_len + sizeof(struct cxmac_hdr),
             (uint8_t *)packetbuf_hdrptr() + csma_hdr_len, payload_len);

      NETSTACK_RADIO.send(data_frame, total_len);
    }
    // turn radio on to receive ACK
    // turn_radio_on();
  }
  if (got_strobe_ack) {
#if XMAC_CONF_USE_CACHE
    register_encounter(packetbuf_addr(PACKETBUF_ADDR_RECEIVER), encounter_time);
    LOG_INFO("Encounter  cache bijgewerkt\n");
#endif
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
      stat_tx_noack++;   /* NIEUW */
    }
    else if (is_broadcast)
    {
      we_are_sending = 0;
      ret = MAC_TX_OK;
      stat_tx_ok++;   /* NIEUW */
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
            LOG_INFO("ACK received\n");
            turn_radio_off();
            ret = MAC_TX_OK;
          }
          else
          {
            LOG_INFO("NO ACK or not for us\n");
            ret = MAC_TX_COLLISION;
          }
        }
      }

      /* NIEUW: teller op basis van het uiteindelijke ret, na de hele if-keten hierboven */
      if (ret == MAC_TX_OK) {
        stat_tx_ok++;
      } else if (ret == MAC_TX_COLLISION) {
        stat_tx_collision++;
      } else {
        stat_tx_noack++;
      }
    }
  }
  else
  {
    someone_is_sending++;
    ret = MAC_TX_COLLISION;
    stat_tx_collision++;   /* NIEUW */
  }

  /* NIEUW: overzichtsregel, telkens aan het einde van de functie */
  LOG_INFO("PACKETS (sender): attempts=%lu ok=%lu noack=%lu collision=%lu err=%lu\n",
           (unsigned long)stat_tx_attempts, (unsigned long)stat_tx_ok,
           (unsigned long)stat_tx_noack, (unsigned long)stat_tx_collision,
           (unsigned long)stat_tx_err);

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

  if (packetbuf_datalen() == 0)
  {
    LOG_DBG("leeg pakket ontvangen (ruis/echo?), genegeerd\n");
    LEDS_OFF(LEDS_RED);
    return;
  }

  if (csma_security_parse_frame() < 0)
  {
    LOG_ERR("failed to parse (%u)\n", packetbuf_datalen());
    LEDS_OFF(LEDS_RED);
    return;
  }

  hdr = packetbuf_dataptr();

  /* Alle xmac-eigen frames (strobe, strobe-ack EN data) hebben nu
     dispatch == DISPATCH als eerste byte. Alles wat dat niet heeft,
     is geen geldig frame voor dit protocol. */
  if (hdr->dispatch != DISPATCH)
  {
    LOG_INFO("Genegeerd: geen geldige dispatch byte\n");
    LEDS_OFF(LEDS_RED);
    return;
  }

  if(packetbuf_datalen() < sizeof(struct cxmac_hdr)){
      LOG_ERR("Packet kleiner dan header. Packet heeft grootte: %u\n", packetbuf_datalen());
      LEDS_OFF(LEDS_RED);
      return;
  }
  if(hdr->checksum != compute_checksum(hdr)) {
      LOG_INFO("CORRUPT: checksum mismatch (verwacht %u, gekregen %u) -> gedropt\n",
              compute_checksum(hdr), hdr->checksum);
      LEDS_OFF(LEDS_RED);
      return;
  }

  LOG_INFO("RX: nieuw frame_id=%u van %02x:%02x\n", hdr->frame_id,
          packetbuf_addr(PACKETBUF_ADDR_SENDER)->u8[6],
          packetbuf_addr(PACKETBUF_ADDR_SENDER)->u8[7]);
  LOG_INFO("received packet: (%u)\n", packetbuf_datalen());

  if (hdr->type == TYPE_STROBE)
  {
    LOG_DBG("header type: strobe\n");
    someone_is_sending = 2;

    if (linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
                     &linkaddr_node_addr))
    {
      LOG_INFO("Received strobe for us\n");

      const linkaddr_t sender_addr = *packetbuf_addr(PACKETBUF_ADDR_SENDER);
      #if XMAC_CONF_USE_CACHE
      rtimer_clock_t strobe_time = RTIMER_NOW();
      #endif

      if (is_duplicate_frame(hdr->frame_id))
      {
        LOG_INFO("RX: DUPLICATE strobe frame_id=%u van %02x:%02x -> genegeerd\n",
                hdr->frame_id, sender_addr.u8[6], sender_addr.u8[7]);
        LEDS_OFF(LEDS_RED);
        return;
      }

      /* onthoud welk frame_id we voor de bijhorende data verwachten. */
      expected_data_frame_id = hdr->frame_id;

      /* This is a strobe packet for us. */
      hdr->type = TYPE_STROBE_ACK;
      packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER,
                         packetbuf_addr(PACKETBUF_ADDR_SENDER));
      packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &linkaddr_node_addr);
      packetbuf_compact();
      if (csma_security_create_frame() >= 0)
      {
        someone_is_sending = 1;
        waiting_for_packet = 1;
        NETSTACK_RADIO.send(packetbuf_hdrptr(), packetbuf_totlen());
        LOG_INFO("send strobe ack (%u)\n", packetbuf_totlen());
        radio_is_on = 1;

#if XMAC_CONF_USE_CACHE
        register_encounter(&sender_addr, strobe_time);
        LOG_INFO("Encounter cache bijgewerkt (ontvangkant) voor %02x:%02x\n",
                sender_addr.u8[6], sender_addr.u8[7]);
#endif
      }
      else
      {
        LOG_INFO("failed to send strobe ack\n");
      }
    }
    else if (linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
                          &linkaddr_null))
    {
      waiting_for_packet = 1;
      turn_radio_on();
    }
    else
    {
      LOG_DBG("strobe not for us\n");
    }

    LEDS_OFF(LEDS_RED);
    return;
  }
  else if (hdr->type == TYPE_STROBE_ACK)
  {
    LOG_DBG("header type: strobe ack\n");
    /* Deze wordt elders (in send_one_packet's strobe-wait-lus)
       afgehandeld, hier niets te doen. */
  }
  else if (hdr->type == TYPE_DATA)
  {
    LOG_DBG("header type: data\n");
    someone_is_sending = 0;

    if (hdr->frame_id != expected_data_frame_id)
    {
      stat_rx_mismatch++;   /* NIEUW */
      LOG_INFO("RX: data frame_id=%u komt niet overeen met verwachte %u -> corrupt/verouderd, gedropt\n",
               hdr->frame_id, expected_data_frame_id);
      LOG_INFO("PACKETS (receiver): delivered=%lu mismatch=%lu\n",
               (unsigned long)stat_rx_delivered, (unsigned long)stat_rx_mismatch);   /* NIEUW */
      LEDS_OFF(LEDS_RED);
      return;
    }

    /* cxmac_hdr eraf halen, zodat wat overblijft de eigenlijke
       applicatiedata is, net zoals voorheen na csma_security_parse_frame(). */
    packetbuf_hdrreduce(sizeof(struct cxmac_hdr));

    if (packetbuf_datalen() == 0)
    {
      LOG_DBG("data packet leeg na parsing (ruis/dubbele RX?), genegeerd\n");
      LEDS_OFF(LEDS_RED);
      return;
    }

    LOG_INFO("received data packet: (%u)\n", packetbuf_datalen());

    if (linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
                     &linkaddr_node_addr) ||
        linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
                     &linkaddr_null))
    {
      const linkaddr_t *data_sender = packetbuf_addr(PACKETBUF_ADDR_SENDER);

      waiting_for_packet = 0;
      stat_rx_delivered++;   /* NIEUW */
      LOG_INFO("PACKETS (receiver): delivered=%lu mismatch=%lu\n",
               (unsigned long)stat_rx_delivered, (unsigned long)stat_rx_mismatch);   /* NIEUW */

#if CSMA_SEND_SOFT_ACK
      uint8_t ackdata[CSMA_ACK_LEN];
      if (packetbuf_attr(PACKETBUF_ATTR_MAC_ACK))
      {
        ackdata[0] = FRAME802154_ACKFRAME;
        ackdata[1] = 0;
        ackdata[2] = ((uint8_t *)packetbuf_hdrptr())[2];
        LOG_DBG("SENDING ACK\n");
        NETSTACK_RADIO.send(ackdata, CSMA_ACK_LEN);
        LOG_INFO("ACK gestuurd naar %02x:%02x voor ontvangen databericht\n",
                data_sender->u8[6], data_sender->u8[7]);
      }
      LOG_DBG("DONE SENDING ACK\n");

      turn_radio_off();
#endif /* CSMA_SEND_SOFT_ACK */

      LOG_DBG("data(%u)\n", packetbuf_datalen());
      NETSTACK_MAC.input();

      LEDS_OFF(LEDS_RED);
      return;
    }
    else
    {
      LOG_INFO("NIET VOOR ONS: pakket van %02x:%02x genegeerd\n",
              packetbuf_addr(PACKETBUF_ADDR_SENDER)->u8[6],
              packetbuf_addr(PACKETBUF_ADDR_SENDER)->u8[7]);
      waiting_for_packet = 0;
    }
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

  LOG_INFO("sizeof(struct cxmac_hdr) = %u\n", (unsigned)sizeof(struct cxmac_hdr));
  radio_is_on = 0;
  waiting_for_packet = 0;
  PT_INIT(&pt);

  list_init(encounter_list);
  memb_init(&encounter_memb);

  frame_counter = random_rand();
  LOG_INFO("frame_counter geinitialiseerd op %u\n", frame_counter);

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

