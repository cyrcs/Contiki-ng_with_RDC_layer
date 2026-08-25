#include "contiki.h"
#include "net/netstack.h"
#include "net/packetbuf.h"
#include "net/linkaddr.h"
#include "sys/energest.h"
#include <stdio.h>
/*
PROCESS(xmac_test_process, "XMAC Test - SENDER");
AUTOSTART_PROCESSES(&xmac_test_process);

PROCESS_THREAD(xmac_test_process, ev, data)
{
  static struct etimer et;
  static linkaddr_t dest_addr;
  static unsigned long packet_count = 0;

  PROCESS_BEGIN();
  dest_addr.u8[0] = 0x00; dest_addr.u8[1] = 0x12;
  dest_addr.u8[2] = 0x4B; dest_addr.u8[3] = 0x00;
  dest_addr.u8[4] = 0x18; dest_addr.u8[5] = 0xEC;
  dest_addr.u8[6] = 0x28; dest_addr.u8[7] = 0xA5;

  while(1) {
    etimer_set(&et, CLOCK_SECOND * 10);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    packetbuf_copyfrom("HELLOHELLO", 10);
    packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, &dest_addr);
    NETSTACK_MAC.send(NULL, NULL);
    packet_count++;
    printf("APP: packet sent\n");

    // --- ENERGEST: cumulatief overzicht sinds boot --- 
    
    energest_flush();
    printf("ENERGEST na %lu packets: CPU=%lu LPM=%lu TX=%lu RX=%lu ticks (RTIMER_SECOND=%u)\n",
           packet_count,
           (unsigned long)energest_type_time(ENERGEST_TYPE_CPU),
           (unsigned long)energest_type_time(ENERGEST_TYPE_LPM),
           (unsigned long)energest_type_time(ENERGEST_TYPE_TRANSMIT),
           (unsigned long)energest_type_time(ENERGEST_TYPE_LISTEN),
           RTIMER_SECOND);
  }
  PROCESS_END();
}

*/
#include "contiki.h"
#include "sys/energest.h"
#include <stdio.h>

PROCESS(xmac_test_process, "XMAC Test - RECEIVER");
AUTOSTART_PROCESSES(&xmac_test_process);

PROCESS_THREAD(xmac_test_process, ev, data)
{
  static struct etimer et;

  PROCESS_BEGIN();

  while(1) {
    etimer_set(&et, CLOCK_SECOND * 10);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    energest_flush();
    printf("ENERGEST (receiver): CPU=%lu LPM=%lu TX=%lu RX=%lu ticks (RTIMER_SECOND=%u)\n",
           (unsigned long)energest_type_time(ENERGEST_TYPE_CPU),
           (unsigned long)energest_type_time(ENERGEST_TYPE_LPM),
           (unsigned long)energest_type_time(ENERGEST_TYPE_TRANSMIT),
           (unsigned long)energest_type_time(ENERGEST_TYPE_LISTEN),
           RTIMER_SECOND);
  }
  PROCESS_END();
}