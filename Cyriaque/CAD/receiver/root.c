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
#define BROADCAST_ADDR 255
#define AMOUNT_OF_PACKETS 10
#define MAX_NEIGHBORS 10
#define MAX_HISTORY 10
#define TYPE_PROBE 5

static int probe_active = 0;
static uint8_t probe_source = 0;

static struct etimer sleep_timer;

#define SLOT_TIME CLOCK_SECOND

typedef struct {
    uint8_t id;
    clock_time_t last_seen;
    uint16_t wake_interval;
} neighbor_t;

static neighbor_t neighbors[MAX_NEIGHBORS];

// FIX: checksum veld toegevoegd aan header
typedef struct {
    uint16_t packet_id;
    uint8_t source;
    uint8_t destination;
    uint8_t type;
    uint8_t checksum;   // ← nieuw: XOR checksum over header + payload
} mac_header_t;

typedef struct {
    mac_header_t header;
    char payload[200];
} mac_dataframe_t;

static uint16_t last_packet_ids[MAX_HISTORY];
// #endregion ------------------------------------------------------------------
// #region Defines--------------------------------------------------------------

static char message[255] = "helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld aa";
static uint16_t global_dataframe_id = 0;

// #region Hulpfuncties conversie ----------------------------------------------
int convert_sf(int sf)
{
    switch (sf)
    {
    case LORA_SF_12: return 12;
    case LORA_SF_7:  return 7;
    case LORA_SF_9:  return 9;
    default:         return -1;
    }
}

int convert_bw(int bw)
{
    switch (bw)
    {
    case LORA_BW_200:  return 200;
    case LORA_BW_1600: return 1600;
    default:           return -1;
    }
}

int convert_cad_symbols(int cad_symbols)
{
    switch (cad_symbols)
    {
    case CAD_SYMBOLS_01: return 1;
    case CAD_SYMBOLS_02: return 2;
    case CAD_SYMBOLS_04: return 4;
    case CAD_SYMBOLS_08: return 8;
    case CAD_SYMBOLS_16: return 16;
    default:             return -1;
    }
}
// #endregion ------------------------------------------------------------------

// #region Checksum ------------------------------------------------------------
// Berekent een XOR checksum over alle header velden (zonder het checksum veld
// zelf) en over de volledige payload.
// Zet header.checksum op 0 vóór je deze functie aanroept bij verificatie.
uint8_t compute_checksum(mac_dataframe_t *frame)
{
    uint8_t cs = 0;

    // Header velden (checksum veld zelf niet meenemen)
    cs ^= (frame->header.packet_id & 0xFF);
    cs ^= ((frame->header.packet_id >> 8) & 0xFF);
    cs ^= frame->header.source;
    cs ^= frame->header.destination;
    cs ^= frame->header.type;

    // Payload
    for (int i = 0; i < (int)strlen(frame->payload); i++)
    {
        cs ^= (uint8_t)frame->payload[i];
    }

    return cs;
}

// Vul het checksum veld in van een frame dat klaar is om te verzenden.
void set_checksum(mac_dataframe_t *frame)
{
    frame->header.checksum = 0;
    frame->header.checksum = compute_checksum(frame);
}

// Controleer het checksum veld van een ontvangen frame.
// Geeft 1 terug als het klopt, 0 als het fout is.
int verify_checksum(mac_dataframe_t *frame)
{
    uint8_t received = frame->header.checksum;
    frame->header.checksum = 0;
    uint8_t computed = compute_checksum(frame);
    frame->header.checksum = received; // herstel zodat je het frame nog kan gebruiken
    return (computed == received);
}
// #endregion ------------------------------------------------------------------

// #region Neighbor / cache ----------------------------------------------------
void update_neighbor(uint8_t id, uint16_t interval)
{
    // Update bestaande buur
    for (int i = 0; i < MAX_NEIGHBORS; i++)
    {
        if (neighbors[i].id == id)
        {
            neighbors[i].last_seen    = clock_time();
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

// Geeft pointer naar neighbor met dit id, of NULL als niet gevonden.
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

int is_neighbor(uint8_t id)
{
    return (get_neighbor(id) != NULL);
}
// #endregion ------------------------------------------------------------------

// #region Verzend functies ----------------------------------------------------
void send_start_packet(int mode)
{
    printf("starting in mode: %d\n", mode);

    mac_dataframe_t dataframe;
    dataframe.header.packet_id   = global_dataframe_id++;
    dataframe.header.source      = NODE_ID;
    dataframe.header.destination = 2;
    dataframe.header.type        = TYPE_START;
    sprintf(dataframe.payload, "start: %d", mode);

    // FIX: checksum instellen voor verzending
    set_checksum(&dataframe);

    sx128x_cmd_set_modulation_params(&SX128X_DEV, LORA_SF_12, LORA_BW_200, LORA_CR_4_8);
    NETSTACK_RADIO.send(&dataframe, sizeof(dataframe));
}

void send_beacon(uint16_t wake_interval)
{
    mac_dataframe_t dataframe;
    dataframe.header.packet_id   = global_dataframe_id++;
    dataframe.header.source      = NODE_ID;
    dataframe.header.destination = BROADCAST_ADDR;
    dataframe.header.type        = TYPE_BEACON;
    sprintf(dataframe.payload, "WAKE:%u", wake_interval);

    // FIX: checksum instellen voor verzending
    set_checksum(&dataframe);

    NETSTACK_RADIO.send(&dataframe, sizeof(dataframe));
    printf("[BEACON SENT] interval = %u\n", wake_interval);
}

void send_probe(uint8_t destination)
{
    mac_dataframe_t probe;
    probe.header.packet_id   = global_dataframe_id++;
    probe.header.source      = NODE_ID;
    probe.header.destination = destination;
    probe.header.type        = TYPE_PROBE;
    sprintf(probe.payload, "PROBE");

    // FIX: checksum instellen voor verzending
    set_checksum(&probe);

    NETSTACK_RADIO.send(&probe, sizeof(probe));
    printf("[PROBE SENT] -> node %d\n", destination);
}
// #endregion ------------------------------------------------------------------

// #region Process -------------------------------------------------------------
PROCESS(node_process, "Shell");
AUTOSTART_PROCESSES(&node_process);

PROCESS_THREAD(node_process, ev, data)
{
    static char buf[255];

    // FIX: drie aparte timers zodat het duidelijk is waarvoor elke timer dient
    static struct etimer cad_timer;  // interval tussen CAD-metingen
    static struct etimer sync_timer; // wachttijd voor zender om klaar te staan
    static struct etimer rx_timer;   // timeout per ontvangen packet

    static int WAIT_TIME;
    static int SF;
    static int BW;
    static int j;
    static int k;
    static int i;

    PROCESS_BEGIN();

    static const int modes[6][3] = {
        {LORA_SF_7,  LORA_BW_1600, 10},
        {LORA_SF_7,  LORA_BW_200,  100},
        {LORA_SF_9,  LORA_BW_1600, 30},
        {LORA_SF_9,  LORA_BW_200,  150},
        {LORA_SF_12, LORA_BW_1600, 125},
        {LORA_SF_12, LORA_BW_200,  1000}
    };

    static const int cads_to_perform[6][5] = {
        {105, 85, 70, 53, 35},
        {195, 134, 86, 52, 30},
        {130, 95, 63, 40, 25},
        {180, 115, 70, 40, 20},
        {160, 110, 67, 37, 22},
        {181, 120, 70, 40, 23},
    };

    static const int arr_CAD_symbols[5]          = {1, 2, 4, 8, 16};
    static const int arr_CAD_symbols_settings[5] = {
        CAD_SYMBOLS_01, CAD_SYMBOLS_02, CAD_SYMBOLS_04,
        CAD_SYMBOLS_08, CAD_SYMBOLS_16
    };

    // -------------------------------------------------------------------------
    // FASE 1: CAD-metingen per mode
    // -------------------------------------------------------------------------
    for (i = 0; i < 6; i++)
    {
        SF        = modes[i][0];
        BW        = modes[i][1];
        WAIT_TIME = modes[i][2];

        printf("SF%d\n", convert_sf(SF));
        printf("BW%d\n", convert_bw(BW));

        for (j = 0; j < 5; j++)
        {
            printf("CAD%d\n", arr_CAD_symbols[j]);

            send_start_packet(i);

            sx128x_cmd_set_cad_params(&SX128X_DEV, arr_CAD_symbols_settings[j]);
            etimer_set(&cad_timer, WAIT_TIME);

            for (k = 0; k < AMOUNT_OF_PACKETS; k++)
            {
                PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&cad_timer));
                etimer_reset(&cad_timer);

                // ! malloc niet aanbevolen, gebruik memb_alloc indien beschikbaar
                int *results = (int *)malloc(cads_to_perform[i][j] * sizeof(int));

                for (int h = 0; h < cads_to_perform[i][j]; h++)
                {
                    results[h] = sx128x_channel_activity_detection();
                }
                for (int h = 0; h < cads_to_perform[i][j]; h++)
                {
                    printf("%d", results[h]);
                }

                // ! free niet aanbevolen, gebruik memb_free indien beschikbaar
                free(results);
                printf("\n");
            }
        }

        // -------------------------------------------------------------------------
        // FASE 2: Echte packet ontvangst
        // -------------------------------------------------------------------------

        // Wacht één extra interval zodat de zender ook naar fase 2 overgaat
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&cad_timer));
        etimer_reset(&cad_timer);

        // Sync-packet: zender weet dat ontvanger klaar is voor RX
        send_start_packet(i);

        // Stel de juiste modulatieparameters in
        sx128x_cmd_set_modulation_params(&SX128X_DEV, SF, BW, LORA_CR_4_5);
        NETSTACK_RADIO.on();

        // FIX: sync_timer geeft de zender tijd om ook klaar te staan
        etimer_set(&sync_timer, WAIT_TIME);
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&sync_timer));

        printf("Wachten op packets:\n");

        for (k = 0; k < AMOUNT_OF_PACKETS; k++)
        {
            // FIX: rx_timer als timeout per packet (was dezelfde cad_timer → verwarrend)
            etimer_set(&rx_timer, WAIT_TIME);
            NETSTACK_RADIO.on();

            // Busy-wait tot packet ontvangen OF timeout
            while ((SX128X_DEV.state.event != SX128X_RX_DONE) && (!etimer_expired(&rx_timer)))
            {
                clock_delay_usec(50);
                watchdog_periodic();
            }

            int len = NETSTACK_RADIO.read(buf, sizeof(buf));

            // FIX 1: lengte check (was al aanwezig, blijft behouden)
            if (len == sizeof(mac_dataframe_t))
            {
                mac_dataframe_t *packet = (mac_dataframe_t *)buf;

                // FIX 2: checksum verificatie
                if (!verify_checksum(packet))
                {
                    printf("[CHECKSUM FOUT] packet verworpen\n");
                    continue;
                }

                if (packet->header.destination == NODE_ID ||
                    packet->header.destination == BROADCAST_ADDR)
                {
                    // FIX 3: duplicate check (was al aanwezig, blijft behouden)
                    if (is_duplicate(packet->header.packet_id))
                    {
                        printf("Duplicate packet -> negeren\n");
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
                        // -----------------------------------------------------
                        case TYPE_BEACON:
                        {
                            printf("-> BEACON ontvangen\n");

                            uint16_t interval = 0;
                            sscanf(packet->payload, "WAKE:%hu", &interval);
                            printf("Wake interval van node %d = %u sec\n",
                                   packet->header.source, interval);

                            // Cache: sla wake-interval op van deze buur
                            update_neighbor(packet->header.source, interval);
                            break;
                        }

                        // -----------------------------------------------------
                        case TYPE_START:
                        {
                            printf("-> START ontvangen\n");
                            break;
                        }

                        // -----------------------------------------------------
                        // FIX: eigen blok {} zodat mac_dataframe_t ack
                        // gedeclareerd kan worden zonder compile-fout
                        case TYPE_DATA:
                        {
                            printf("-> DATA ontvangen\n");

                            if (!probe_active || packet->header.source != probe_source)
                            {
                                printf("DATA geweigerd (geen actieve PROBE van deze node)\n");
                                break;
                            }

                            mac_dataframe_t ack;
                            ack.header.packet_id   = global_dataframe_id++;
                            ack.header.source      = NODE_ID;
                            ack.header.destination = packet->header.source;
                            ack.header.type        = TYPE_ACK;
                            sprintf(ack.payload, "ACK");

                            // FIX: checksum instellen voor verzending
                            set_checksum(&ack);

                            NETSTACK_RADIO.send(&ack, sizeof(mac_dataframe_t));

                            probe_active = 0;
                            probe_source = 0;
                            break;
                        }

                        // -----------------------------------------------------
                        case TYPE_ACK:
                        {
                            printf("-> ACK ontvangen\n");
                            break;
                        }

                        // -----------------------------------------------------
                        // FIX: eigen blok {} zodat mac_dataframe_t ack hier
                        // opnieuw gedeclareerd kan worden zonder naamconflict
                        case TYPE_PROBE:
                        {
                            printf("-> PROBE ontvangen\n");

                            probe_active = 1;
                            probe_source = packet->header.source;

                            mac_dataframe_t ack;
                            ack.header.packet_id   = global_dataframe_id++;
                            ack.header.source      = NODE_ID;
                            ack.header.destination = packet->header.source;
                            ack.header.type        = TYPE_ACK;
                            sprintf(ack.payload, "READY");

                            // FIX: checksum instellen voor verzending
                            set_checksum(&ack);

                            NETSTACK_RADIO.send(&ack, sizeof(mac_dataframe_t));

                            printf("[READY gestuurd naar %d]\n", packet->header.source);
                            break;
                        }

                        // -----------------------------------------------------
                        default:
                        {
                            printf("-> ONBEKEND TYPE\n");
                            break;
                        }
                    }
                }
                else
                {
                    // Packet is niet voor ons → doorsturen als bestemming een buur is
                    printf("Packet niet voor deze node\n");

                    if (is_neighbor(packet->header.destination))
                    {
                        printf("Bestemming is een buur → doorsturen\n");

                        // FIX: sizeof(packet) was grootte van pointer (4 bytes!)
                        // Correct: sizeof(mac_dataframe_t)
                        NETSTACK_RADIO.send(packet, sizeof(mac_dataframe_t));
                    }
                }
            }
            else
            {
                printf("Foute packet grootte: %d bytes (verwacht: %d)\n",
                       len, (int)sizeof(mac_dataframe_t));
            }
        } // einde RX packet loop

        // -------------------------------------------------------------------------
        // Slaap tot volgende slot-boundary als er geen actieve probe is.
        // Wake-up mechanisme: als de bestemming een gekende buur is, wacht dan
        // tot het volgende verwachte wake-moment van die node voor je stuurt.
        // -------------------------------------------------------------------------
        if (!probe_active)
        {
            clock_time_t now       = clock_time();
            clock_time_t next_slot = ((now / SLOT_TIME) + 1) * SLOT_TIME;

            printf("Idle → slapen tot slot boundary\n");

            etimer_set(&sleep_timer, next_slot - now);
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&sleep_timer));
        }

        printf("\n");

        // Wacht het resterende interval van cad_timer af voor de volgende mode
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&cad_timer));
        etimer_reset(&cad_timer);

    } // einde mode loop

    PROCESS_END();
}
// #endregion ------------------------------------------------------------------