/**
 * \file
 *         A MAC protocol that does not do anything.
 * \author
 *         Adam Dunkels <adam@sics.se>
 *
 * Changes made by
 *          Louka Grignard <louka.michael.grignard@vub.be>
 */

#include "net/mac/rdc/nullrdc-noframer/nullrdc-noframer.h"
#include "net/packetbuf.h"
#include "net/queuebuf.h"
#include "net/netstack.h"
#include <string.h>

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "NULLRDC NOFRAMER"
#define LOG_LEVEL LOG_LEVEL_RDC

/*---------------------------------------------------------------------------*/
static void
send_packet(mac_callback_t sent, void *ptr)
{
  LOG_FUNC("Function call: %s\n", __func__);

  int ret;
  if (NETSTACK_RADIO.send(packetbuf_hdrptr(), packetbuf_totlen()) == RADIO_TX_OK)
  {
    ret = MAC_TX_OK;
  }
  else
  {
    ret = MAC_TX_ERR;
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
  NETSTACK_MAC.input();
}
/*---------------------------------------------------------------------------*/
static int
on(void)
{
  LOG_FUNC("Function call: %s\n", __func__);

  // work around to print the returned integer value
  // int val;
  // val = NETSTACK_RADIO.on();
  // LOG_INFO("VAL: %d\n", val);
  // return val;

  return NETSTACK_RADIO.on();
}
/*---------------------------------------------------------------------------*/
static int
off(void)
{
  LOG_FUNC("Function call: %s\n", __func__);
  return NETSTACK_RADIO.off();
}
/*---------------------------------------------------------------------------*/
static unsigned short
channel_check_interval(void)
{
  LOG_FUNC("Function call: %s\n", __func__);
  return 0;
}
/*---------------------------------------------------------------------------*/
static void
init(void)
{
  LOG_FUNC("Function call: %s\n", __func__);
  on();
}
/*---------------------------------------------------------------------------*/
const struct rdc_driver nullrdc_noframer_driver = {
    "nullrdc-noframer",
    init,
    send_packet,
    send_list,
    packet_input,
    on,
    off,
    channel_check_interval,
};
/*---------------------------------------------------------------------------*/
