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

#define NODE_ID        2
#define BROADCAST_ADDR 255

#define TYPE_START  1
#define TYPE_DATA   2
#define TYPE_ACK    3
#define TYPE_BEACON 4
#define TYPE_PROBE  5

#define WAKE_INTERVAL  (5 * CLOCK_SECOND)
#define LISTEN_WINDOW  (CLOCK_SECOND)
#define PROBE_TIMEOUT  (2 * CLOCK_SECOND)
#define DATA_TIMEOUT   (2 * CLOCK_SECOND)

#define MAX_NEIGHBORS 10
#define MAX_HISTORY   10
// #endregion ------------------------------------------------------------------

// #region Structs -------------------------------------------------------------
typedef struct {
    uint8_t id;
    clock_time_t last_seen;
    uint16_t wake_interval;
} neighbor_t;

typedef struct {
    uint16_t packet_id;
    uint8_t  source;
    uint8_t  destination;
    uint8_t  type;
    uint8_t  checksum;
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

static struct etimer discovery_timer;
static struct etimer sleep_timer;
static struct etimer probe_timer;
static struct etimer data_timer;
// #endregion ------------------------------------------------------------------

// #region Checksum ------------------------------------------------------------
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

void set_checksum(mac_dataframe_t *frame)
{
    frame->header.checksum = 0;
    frame->header.checksum = compute_checksum(frame);
}

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
    for (int i = 0; i < MAX_NEIGHBORS; i++)
    {
        if (neighbors[i].id == id)
        {
            neighbors[i].last_seen     = clock_time();
            neighbors[i].wake_interval = interval;
            return;
        }
    }
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

// Berekent hoeveel ticks tot het volgende wake-moment van een buur.
// Geeft 0 terug als de buur onbekend is.
clock_time_t time_until_neighbor_wakes(uint8_t id)
{
    neighbor_t *nb = get_neighbor(id);
    if (nb == NULL || nb->wake_interval == 0)
    {
        return 0;
    }
    clock_time_t now           = clock_time();
    clock_time_t elapsed       = now - nb->last_seen;
    clock_time_t interval      = nb->wake_interval;
    clock_time_t time_in_cycle = elapsed % interval;
    clock_time_t wait          = interval - time_in_cycle;
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
    printf("[BEACON SENT] interval = %u ticks\n", wake_interval);
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
// #endregion ------------------------------------------------------------------

// #region Hulpfunctie: ontvang en verwerk één packet --------------------------
// Leest een packet, controleert lengte + checksum, verwerkt beacons automatisch.
// Geeft pointer terug naar geldig packet voor ons, anders NULL.
mac_dataframe_t *try_receive(char *buf, int buf_size)
{
    int len = NETSTACK_RADIO.read(buf, buf_size);

    if (len != sizeof(mac_dataframe_t))
    {
        printf("[RX] Foute lengte: %d bytes (verwacht: %d)\n",
               len, (int)sizeof(mac_dataframe_t));
        return NULL;
    }

    mac_dataframe_t *packet = (mac_dataframe_t *)buf;

    if (!verify_checksum(packet))
    {
        printf("[RX] Checksum fout → verworpen\n");
        return NULL;
    }

    // Beacon altijd verwerken zodat cache up-to-date blijft
    if (packet->header.type == TYPE_BEACON)
    {
        uint16_t interval = 0;
        sscanf(packet->payload, "WAKE:%hu", &interval);
        update_neighbor(packet->header.source, interval);
        printf("[BEACON] Wake interval node %d = %u ticks\n",
               packet->header.source, interval);
    }

    // Alleen packets voor ons teruggeven
    if (packet->header.destination != NODE_ID &&
        packet->header.destination != BROADCAST_ADDR)
    {
        return NULL;
    }

    if (is_duplicate(packet->header.packet_id))
    {
        printf("[RX] Duplicate → negeren\n");
        return NULL;
    }

    return packet;
}
// #endregion ------------------------------------------------------------------

// #region Process -------------------------------------------------------------
PROCESS(node_process, "MAC Zender");
AUTOSTART_PROCESSES(&node_process);

PROCESS_THREAD(node_process, ev, data)
{
    static char buf[255];
    static uint8_t target     = 1;   // node waarnaar we sturen (receiver = 1)
    static int ready_received = 0;
    static int ack_received   = 0;

    PROCESS_BEGIN();

    printf("[BOOT] Zender node %d gestart\n", NODE_ID);

    // Zet radio aan en stuur eigen beacon zodat buren ons al kennen
    NETSTACK_RADIO.on();
    send_beacon(WAKE_INTERVAL);

    // =========================================================================
    // DISCOVERY FASE
    // Luister in blokken van WAKE_INTERVAL totdat we een beacon hebben van
    // onze bestemming. Zonder dat weten we niet wanneer die wakker is.
    // =========================================================================
    printf("[DISCOVERY] Wachten op beacon van node %d...\n", target);

    while (get_neighbor(target) == NULL)
    {
        // Luister gedurende één wake-interval
        etimer_set(&discovery_timer, WAKE_INTERVAL);

        while (!etimer_expired(&discovery_timer))
        {
            if (SX128X_DEV.state.event == SX128X_RX_DONE)
            {
                // try_receive verwerkt de beacon automatisch in de cache
                try_receive(buf, sizeof(buf));
            }
            else
            {
                clock_delay_usec(50);
                watchdog_periodic();
            }
        }

        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&discovery_timer));

        if (get_neighbor(target) == NULL)
        {
            printf("[DISCOVERY] Nog geen beacon van node %d, opnieuw wachten...\n", target);
            // Stuur eigen beacon opnieuw zodat receiver ons ook leert kennen
            send_beacon(WAKE_INTERVAL);
        }
    }

    printf("[DISCOVERY] Klaar! Wake-interval van node %d is gekend.\n", target);

    // =========================================================================
    // HOOFDLUS: stuur data naar bestemming in elke cyclus
    // =========================================================================
    while (1)
    {
        // ---------------------------------------------------------------------
        // STAP 1: Wacht tot bestemming wakker is
        // We berekenen op basis van de cache wanneer het volgende wake-moment is.
        // ---------------------------------------------------------------------
        clock_time_t wait = time_until_neighbor_wakes(target);

        if (wait > 0)
        {
            printf("[SLEEP] Wachten %lu ticks tot node %d wakker is\n",
                   (unsigned long)wait, target);

            NETSTACK_RADIO.off();
            etimer_set(&sleep_timer, wait);
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&sleep_timer));
            NETSTACK_RADIO.on();
        }

        // ---------------------------------------------------------------------
        // STAP 2: Stuur PROBE en wacht op READY
        // ---------------------------------------------------------------------
        ready_received = 0;
        send_probe(target);
        etimer_set(&probe_timer, PROBE_TIMEOUT);

        while (!ready_received && !etimer_expired(&probe_timer))
        {
            if (SX128X_DEV.state.event == SX128X_RX_DONE)
            {
                mac_dataframe_t *packet = try_receive(buf, sizeof(buf));

                if (packet != NULL                          &&
                    packet->header.type   == TYPE_ACK       &&
                    packet->header.source == target         &&
                    strcmp(packet->payload, "READY") == 0)
                {
                    printf("[PROBE] READY ontvangen van node %d\n", target);
                    ready_received = 1;
                }
            }
            else
            {
                clock_delay_usec(50);
                watchdog_periodic();
            }
        }

        if (!ready_received)
        {
            printf("[PROBE] Geen READY ontvangen, opnieuw proberen na %lu ticks\n",
                   (unsigned long)WAKE_INTERVAL);
            etimer_set(&sleep_timer, WAKE_INTERVAL);
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&sleep_timer));
            continue;
        }

        // ---------------------------------------------------------------------
        // STAP 3: Stuur DATA en wacht op ACK
        // ---------------------------------------------------------------------
        ack_received = 0;
        send_data(target, "helloworld");
        etimer_set(&data_timer, DATA_TIMEOUT);

        while (!ack_received && !etimer_expired(&data_timer))
        {
            if (SX128X_DEV.state.event == SX128X_RX_DONE)
            {
                mac_dataframe_t *packet = try_receive(buf, sizeof(buf));

                if (packet != NULL                        &&
                    packet->header.type   == TYPE_ACK     &&
                    packet->header.source == target       &&
                    strcmp(packet->payload, "ACK") == 0)
                {
                    printf("[DATA] ACK ontvangen van node %d\n", target);
                    ack_received = 1;
                }
            }
            else
            {
                clock_delay_usec(50);
                watchdog_periodic();
            }
        }

        if (!ack_received)
        {
            printf("[DATA] Geen ACK ontvangen van node %d\n", target);
        }
        else
        {
            printf("[OK] Data succesvol afgeleverd aan node %d\n", target);
        }

        // ---------------------------------------------------------------------
        // STAP 4: Slaap tot volgende cyclus
        // ---------------------------------------------------------------------
        NETSTACK_RADIO.off();
        printf("[SLEEP] Slapen voor %lu ticks\n", (unsigned long)WAKE_INTERVAL);

        etimer_set(&sleep_timer, WAKE_INTERVAL);
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&sleep_timer));
    }

    PROCESS_END();
}
// #endregion ------------------------------------------------------------------