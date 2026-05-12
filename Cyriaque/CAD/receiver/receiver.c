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
// #endregion ------------------------------------------------------------------

// #region Defines -------------------------------------------------------------
#define LOG_MODULE "MAIN"
#define LOG_LEVEL LOG_LEVEL_DBG

#define NODE_ID        1
#define BROADCAST_ADDR 255

// Packet types
#define TYPE_START  1
#define TYPE_DATA   2
#define TYPE_ACK    3
#define TYPE_BEACON 4
#define TYPE_PROBE  5

// Timing
#define WAKE_INTERVAL  (5 * CLOCK_SECOND)   // hoe vaak deze node wakker wordt
#define LISTEN_WINDOW  (CLOCK_SECOND)        // hoe lang luisteren per wake-cyclus
#define PROBE_TIMEOUT  (2 * CLOCK_SECOND)    // hoe lang wachten op ACK na PROBE
#define DATA_TIMEOUT   (2 * CLOCK_SECOND)    // hoe lang wachten op ACK na DATA
#define SLOT_TIME      CLOCK_SECOND

// Limieten
#define MAX_NEIGHBORS 10
#define MAX_HISTORY   10
// #endregion ------------------------------------------------------------------

// #region Structs -------------------------------------------------------------
typedef struct {
    uint8_t id;
    clock_time_t last_seen;
    uint16_t wake_interval;  // wake-interval van deze buur in clock ticks
} neighbor_t;

typedef struct {
    uint16_t packet_id;
    uint8_t  source;
    uint8_t  destination;
    uint8_t  type;
    uint8_t  checksum;       // XOR checksum over header + payload
} mac_header_t;

typedef struct {
    mac_header_t header;
    char payload[200];
} mac_dataframe_t;
// #endregion ------------------------------------------------------------------

// #region Globale state -------------------------------------------------------
static uint16_t global_dataframe_id = 0;

static neighbor_t neighbors[MAX_NEIGHBORS];
static uint16_t   last_packet_ids[MAX_HISTORY];

static int     probe_active = 0;
static uint8_t probe_source = 0;

static struct etimer rx_timer;
static struct etimer sleep_timer;
//static struct etimer probe_timer;
//static struct etimer data_timer;
// #endregion ------------------------------------------------------------------

// #region Checksum ------------------------------------------------------------
// Berekent XOR checksum over alle header velden (zonder checksum zelf) + payload
uint8_t compute_checksum(mac_dataframe_t *frame)
{
    uint8_t cs = 0;
    cs ^= (frame->header.packet_id & 0xFF);
    cs ^= ((frame->header.packet_id >> 8) & 0xFF);
    cs ^= frame->header.source;
    cs ^= frame->header.destination;
    cs ^= frame->header.type;
    for (int i = 0; i < (int)strlen(frame->payload); i++)
    {
        cs ^= (uint8_t)frame->payload[i];
    }
    return cs;
}

// Vul checksum in van een frame dat klaar is om te verzenden
void set_checksum(mac_dataframe_t *frame)
{
    frame->header.checksum = 0;
    frame->header.checksum = compute_checksum(frame);
}

// Controleer checksum van ontvangen frame. Geeft 1 terug als OK, 0 als fout.
int verify_checksum(mac_dataframe_t *frame)
{
    uint8_t received = frame->header.checksum;
    frame->header.checksum = 0;
    uint8_t computed = compute_checksum(frame);
    frame->header.checksum = received;
    return (computed == received);
}
// #endregion ------------------------------------------------------------------

// #region Neighbor cache ------------------------------------------------------
void update_neighbor(uint8_t id, uint16_t interval)
{
    // Update bestaande buur
    for (int i = 0; i < MAX_NEIGHBORS; i++)
    {
        if (neighbors[i].id == id)
        {
            neighbors[i].last_seen     = clock_time();
            neighbors[i].wake_interval = interval;
            return;
        }
    }
    // Voeg nieuwe buur toe
    for (int i = 0; i < MAX_NEIGHBORS; i++)
    {
        if (neighbors[i].id == 0)
        {
            neighbors[i].id            = id;
            neighbors[i].last_seen     = clock_time();
            neighbors[i].wake_interval = interval;
            return;
        }
    }
    printf("[WARN] Neighbor tabel vol\n");
}

// Geeft pointer naar buur met dit id, of NULL als niet gevonden
neighbor_t *get_neighbor(uint8_t id)
{
    for (int i = 0; i < MAX_NEIGHBORS; i++)
    {
        if (neighbors[i].id == id)
        {
            return &neighbors[i];
        }
    }
    return NULL;
}

int is_neighbor(uint8_t id)
{
    return (get_neighbor(id) != NULL);
}

// Berekent hoeveel clock ticks tot het volgende verwachte wake-moment van een buur.
// Geeft 0 terug als de buur onbekend is (stuur dan meteen).
clock_time_t time_until_neighbor_wakes(uint8_t id)
{
    neighbor_t *nb = get_neighbor(id);
    if (nb == NULL || nb->wake_interval == 0)
    {
        return 0;
    }

    clock_time_t now          = clock_time();
    clock_time_t elapsed      = now - nb->last_seen;
    clock_time_t interval     = nb->wake_interval;
    clock_time_t time_in_cycle = elapsed % interval;
    clock_time_t wait         = interval - time_in_cycle;

    return wait;
}
// #endregion ------------------------------------------------------------------

// #region Duplicate detection -------------------------------------------------
int is_duplicate(uint16_t id)
{
    for (int i = 0; i < MAX_HISTORY; i++)
    {
        if (last_packet_ids[i] == id)
        {
            return 1;
        }
    }
    // Shift en voeg toe aan het begin
    for (int i = MAX_HISTORY - 1; i > 0; i--)
    {
        last_packet_ids[i] = last_packet_ids[i - 1];
    }
    last_packet_ids[0] = id;
    return 0;
}
// #endregion ------------------------------------------------------------------

// #region Verzend functies ----------------------------------------------------
void send_beacon(uint16_t wake_interval)
{
    mac_dataframe_t frame;
    frame.header.packet_id   = global_dataframe_id++;
    frame.header.source      = NODE_ID;
    frame.header.destination = BROADCAST_ADDR;
    frame.header.type        = TYPE_BEACON;
    sprintf(frame.payload, "WAKE:%u", wake_interval);
    set_checksum(&frame);

    NETSTACK_RADIO.send(&frame, sizeof(mac_dataframe_t));
    printf("[BEACON SENT] interval = %u\n", wake_interval);
}

void send_probe(uint8_t destination)
{
    mac_dataframe_t frame;
    frame.header.packet_id   = global_dataframe_id++;
    frame.header.source      = NODE_ID;
    frame.header.destination = destination;
    frame.header.type        = TYPE_PROBE;
    sprintf(frame.payload, "PROBE");
    set_checksum(&frame);

    NETSTACK_RADIO.send(&frame, sizeof(mac_dataframe_t));
    printf("[PROBE SENT] -> node %d\n", destination);
}

void send_data(uint8_t destination, char *payload)
{
    mac_dataframe_t frame;
    frame.header.packet_id   = global_dataframe_id++;
    frame.header.source      = NODE_ID;
    frame.header.destination = destination;
    frame.header.type        = TYPE_DATA;
    sprintf(frame.payload, "%s", payload);
    set_checksum(&frame);

    NETSTACK_RADIO.send(&frame, sizeof(mac_dataframe_t));
    printf("[DATA SENT] -> node %d\n", destination);
}

void send_ack(uint8_t destination, char *payload_msg)
{
    mac_dataframe_t frame;
    frame.header.packet_id   = global_dataframe_id++;
    frame.header.source      = NODE_ID;
    frame.header.destination = destination;
    frame.header.type        = TYPE_ACK;
    sprintf(frame.payload, "%s", payload_msg);
    set_checksum(&frame);

    NETSTACK_RADIO.send(&frame, sizeof(mac_dataframe_t));
    printf("[ACK SENT] -> node %d : %s\n", destination, payload_msg);
}
// #endregion ------------------------------------------------------------------

// #region Process -------------------------------------------------------------
PROCESS(node_process, "MAC Node");
AUTOSTART_PROCESSES(&node_process);

PROCESS_THREAD(node_process, ev, data)
{
    static char buf[255];

    PROCESS_BEGIN();

    printf("[BOOT] Node %d gestart\n", NODE_ID);

    // Stuur beacon zodat buren ons wake-interval kennen
    send_beacon(WAKE_INTERVAL);

    while (1)
    {
        // -----------------------------------------------------------------
        // Luisterfase: zet radio aan en luister gedurende LISTEN_WINDOW
        // -----------------------------------------------------------------
        NETSTACK_RADIO.on();
        etimer_set(&rx_timer, LISTEN_WINDOW);

        printf("[AWAKE] Luisterfase gestart\n");

        while (!etimer_expired(&rx_timer))
        {
            // Wacht op RX event of timeout
            if (SX128X_DEV.state.event == SX128X_RX_DONE)
            {
                int len = NETSTACK_RADIO.read(buf, sizeof(buf));

                // Detectie 1: lengte check
                if (len != sizeof(mac_dataframe_t))
                {
                    printf("[RX] Foute lengte: %d bytes (verwacht: %d)\n",
                           len, (int)sizeof(mac_dataframe_t));
                    continue;
                }

                mac_dataframe_t *packet = (mac_dataframe_t *)buf;

                // Detectie 2: checksum verificatie
                if (!verify_checksum(packet))
                {
                    printf("[RX] Checksum fout → packet verworpen\n");
                    continue;
                }

                // Packet is niet voor ons → doorsturen als bestemming een buur is
                if (packet->header.destination != NODE_ID &&
                    packet->header.destination != BROADCAST_ADDR)
                {
                    printf("[RX] Packet niet voor deze node\n");
                    if (is_neighbor(packet->header.destination))
                    {
                        printf("[FORWARD] Doorsturen naar node %d\n",
                               packet->header.destination);
                        NETSTACK_RADIO.send(packet, sizeof(mac_dataframe_t));
                    }
                    continue;
                }

                // Detectie 3: duplicate check
                if (is_duplicate(packet->header.packet_id))
                {
                    printf("[RX] Duplicate packet → negeren\n");
                    continue;
                }

                printf("\n--- PACKET ONTVANGEN ---\n");
                printf("Source:      %d\n", packet->header.source);
                printf("Destination: %d\n", packet->header.destination);
                printf("Type:        %d\n", packet->header.type);
                printf("Packet ID:   %d\n", packet->header.packet_id);
                printf("Payload:     %s\n", packet->payload);

                switch (packet->header.type)
                {
                    // ---------------------------------------------------------
                    case TYPE_BEACON:
                    {
                        printf("-> BEACON ontvangen\n");

                        uint16_t interval = 0;
                        sscanf(packet->payload, "WAKE:%hu", &interval);
                        printf("Wake interval van node %d = %u ticks\n",
                               packet->header.source, interval);

                        // Cache: sla wake-interval op van deze buur
                        update_neighbor(packet->header.source, interval);
                        break;
                    }

                    // ---------------------------------------------------------
                    case TYPE_PROBE:
                    {
                        printf("-> PROBE ontvangen\n");

                        probe_active = 1;
                        probe_source = packet->header.source;

                        // Stuur READY terug zodat zender weet dat we luisteren
                        send_ack(packet->header.source, "READY");
                        printf("[READY gestuurd naar %d]\n", packet->header.source);
                        break;
                    }

                    // ---------------------------------------------------------
                    case TYPE_DATA:
                    {
                        printf("-> DATA ontvangen\n");

                        if (!probe_active || packet->header.source != probe_source)
                        {
                            printf("[RX] DATA geweigerd (geen actieve PROBE van deze node)\n");
                            break;
                        }

                        // Stuur ACK terug
                        send_ack(packet->header.source, "ACK");

                        probe_active = 0;
                        probe_source = 0;
                        break;
                    }

                    // ---------------------------------------------------------
                    case TYPE_ACK:
                    {
                        printf("-> ACK ontvangen: %s\n", packet->payload);
                        break;
                    }

                    // ---------------------------------------------------------
                    case TYPE_START:
                    {
                        printf("-> START ontvangen\n");
                        break;
                    }

                    // ---------------------------------------------------------
                    default:
                    {
                        printf("-> ONBEKEND TYPE\n");
                        break;
                    }
                }
            }
            else
            {
                clock_delay_usec(50);
                watchdog_periodic();
            }
        }

        // -----------------------------------------------------------------
        // Slaapfase: radio uit, wacht tot volgend wake-moment
        // Als er een actieve probe is, niet slapen zodat DATA ontvangen kan worden
        // -----------------------------------------------------------------
        if (!probe_active)
        {
            NETSTACK_RADIO.off();
            printf("[SLEEP] Slapen voor %lu ticks\n", (unsigned long)WAKE_INTERVAL);

            etimer_set(&sleep_timer, WAKE_INTERVAL);
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&sleep_timer));
        }
        else
        {
            printf("[AWAKE] Probe actief, niet slapen\n");
        }
    }

    PROCESS_END();
}
// #endregion ------------------------------------------------------------------