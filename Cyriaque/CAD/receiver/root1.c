// #region Libraries -----------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include "contiki.h"
#include "dev/serial-line.h"
#include "dev/uart.h"
#include "dev/spi.h"
#include "netstack.h"
#include "process.h"
#include "rtimer-arch.h"
#include "shell.h"
#include "shell-commands.h"
#include "sys/_stdint.h"
#include "sys/log.h"
#include "sx128x.h"
#include "antenna-sw.h"
#include "string.h"
#include "sys/etimer.h"

#define LOG_MODULE "MAIN"
#define LOG_LEVEL LOG_LEVEL_DBG

#define NODE_ID 1

#define TYPE_START 1
#define TYPE_DATA 2
#define TYPE_ACK 3
#define TYPE_BEACON 4
#define TYPE_PROBE 5

#define BROADCAST_ADDR 255
#define AMOUNT_OF_PACKETS 10
#define MAX_NEIGHBORS 10
#define MAX_HISTORY 10

#define SLOT_TIME CLOCK_SECOND

// ---------------- STATE ----------------
static int probe_active = 0;
static uint8_t probe_source = 0;

static struct etimer sleep_timer;

// ---------------- NEIGHBORS ----------------
typedef struct {
    uint8_t id;
    clock_time_t last_seen;
    uint16_t wake_interval;
} neighbor_t;

static neighbor_t neighbors[MAX_NEIGHBORS];

// ---------------- PACKET STRUCT ----------------
typedef struct {
    uint16_t packet_id;
    uint8_t source;
    uint8_t destination;
    uint8_t type;
} mac_header_t;

typedef struct{
    mac_header_t header;
    char payload[200];
} mac_dataframe_t;

static uint16_t last_packet_ids[MAX_HISTORY];
static uint16_t global_dataframe_id = 0;

// ---------------- UTIL ----------------
int is_neighbor(uint8_t id)
{
    for(int i = 0; i < MAX_NEIGHBORS; i++)
        if(neighbors[i].id == id)
            return 1;
    return 0;
}

int is_duplicate(uint16_t id)
{
    for(int i = 0; i < MAX_HISTORY; i++)
        if(last_packet_ids[i] == id)
            return 1;

    for(int i = MAX_HISTORY - 1; i > 0; i--)
        last_packet_ids[i] = last_packet_ids[i - 1];

    last_packet_ids[0] = id;
    return 0;
}

void update_neighbor(uint8_t id, uint16_t interval)
{
    for(int i = 0; i < MAX_NEIGHBORS; i++)
    {
        if(neighbors[i].id == id)
        {
            neighbors[i].last_seen = clock_time();
            neighbors[i].wake_interval = interval;
            return;
        }
    }

    for(int i = 0; i < MAX_NEIGHBORS; i++)
    {
        if(neighbors[i].id == 0)
        {
            neighbors[i].id = id;
            neighbors[i].last_seen = clock_time();
            neighbors[i].wake_interval = interval;
            return;
        }
    }
}

// ---------------- PROBE SEND ----------------
void send_probe(uint8_t destination)
{
    mac_dataframe_t probe;

    probe.header.packet_id = global_dataframe_id++;
    probe.header.source = NODE_ID;
    probe.header.destination = destination;
    probe.header.type = TYPE_PROBE;

    sprintf(probe.payload, "PROBE");

    NETSTACK_RADIO.send(&probe, sizeof(probe));

    printf("[PROBE SENT] -> node %d\n", destination);
}

// ---------------- START SEND ----------------
void send_start_packet(int mode)
{
    mac_dataframe_t dataframe;

    dataframe.header.packet_id = global_dataframe_id++;
    dataframe.header.source = NODE_ID;
    dataframe.header.destination = 2;
    dataframe.header.type = TYPE_START;

    sprintf(dataframe.payload, "start: %d", mode);

    sx128x_cmd_set_modulation_params(&SX128X_DEV, LORA_SF_12, LORA_BW_200, LORA_CR_4_8);
    NETSTACK_RADIO.send(&dataframe, sizeof(dataframe));
}

// ---------------- PROCESS ----------------
PROCESS(node_process, "Node process");
AUTOSTART_PROCESSES(&node_process);

PROCESS_THREAD(node_process, ev, data)
{
    static char buf[255];

    static struct etimer et;

    PROCESS_BEGIN();

    for(int i = 0; i < 6; i++)
    {
        send_start_packet(i);

        etimer_set(&et, CLOCK_SECOND);

        for(int k = 0; k < AMOUNT_OF_PACKETS; k++)
        {
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
            etimer_reset(&et);

            int len = NETSTACK_RADIO.read(buf, sizeof(buf));

            if(len == sizeof(mac_dataframe_t))
            {
                mac_dataframe_t *packet = (mac_dataframe_t *) buf;

                if(packet->destination == NODE_ID || packet->destination == BROADCAST_ADDR)
                {
                    if(is_duplicate(packet->packet_id))
                        continue;

                    printf("\n--- PACKET ---\n");

                    switch(packet->type)
                    {
                        case TYPE_BEACON:
                        {
                            uint16_t interval = 0;
                            sscanf(packet->payload, "WAKE:%hu", &interval);
                            update_neighbor(packet->source, interval);
                            break;
                        }

                        case TYPE_START:
                        {
                            printf("START ontvangen\n");
                            break;
                        }

                        case TYPE_PROBE:
                        {
                            printf("PROBE ontvangen\n");

                            probe_active = 1;
                            probe_source = packet->source;

                            mac_dataframe_t ack;
                            ack.header.packet_id = packet->packet_id;
                            ack.header.source = NODE_ID;
                            ack.header.destination = packet->source;
                            ack.header.type = TYPE_ACK;

                            sprintf(ack.payload, "READY");

                            NETSTACK_RADIO.send(&ack, sizeof(ack));

                            break;
                        }

                        case TYPE_DATA:
                        {
                            printf("DATA ontvangen\n");

                            if(!probe_active || packet->source != probe_source)
                            {
                                printf("DATA geweigerd\n");
                                break;
                            }

                            mac_dataframe_t ack;
                            ack.header.packet_id = packet->packet_id;
                            ack.header.source = NODE_ID;
                            ack.header.destination = packet->source;
                            ack.header.type = TYPE_ACK;

                            sprintf(ack.payload, "ACK");

                            NETSTACK_RADIO.send(&ack, sizeof(ack));

                            probe_active = 0;
                            probe_source = 0;

                            break;
                        }

                        case TYPE_ACK:
                        {
                            printf("ACK ontvangen\n");
                            break;
                        }
                    }
                }
            }
        }

        // ---------------- SLOT-ALIGNED SLEEP (CORRECT PLACE) ----------------
        if(!probe_active)
        {
            clock_time_t now = clock_time();

            clock_time_t next_slot =
                ((now / SLOT_TIME) + 1) * SLOT_TIME;

            clock_time_t sleep_time = next_slot - now;

            printf("Idle → slapen tot slot\n");

            etimer_set(&sleep_timer, sleep_time);
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&sleep_timer));
        }

        printf("\n");
    }

    PROCESS_END();
}