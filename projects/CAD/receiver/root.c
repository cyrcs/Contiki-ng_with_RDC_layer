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
// #region Defines--------------------------------------------------------------
#define LOG_MODULE "MAIN"
#define LOG_LEVEL LOG_LEVEL_DBG

#define AMOUNT_OF_PACKETS 10

static char message[255] = "helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld aa";

// ------------------------------- refactor -----------------------------------
int convert_sf(int sf)
{
    switch (sf)
    {
    case LORA_SF_12:
        return 12;
    case LORA_SF_7:
        return 7;
    case LORA_SF_9:
        return 9;
    default:
        return -1;
    }
}
int convert_bw(int bw)
{
    switch (bw)
    {
    case LORA_BW_200:
        return 200;
    case LORA_BW_1600:
        return 1600;
    default:
        return -1;
    }
}
int convert_cad_symbols(int cad_symbols)
{
    switch (cad_symbols)
    {
    case CAD_SYMBOLS_01:
        return 1;
    case CAD_SYMBOLS_02:
        return 2;
    case CAD_SYMBOLS_04:
        return 4;
    case CAD_SYMBOLS_08:
        return 8;
    case CAD_SYMBOLS_16:
        return 16;
    default:
        return -1;
    }
}

void send_start_packet(int mode)
{
    // printf("starting in mode: %d\n", mode);

    char message[255];
    sprintf(message, "start: %d", mode);

    // use default parameters
    sx128x_cmd_set_modulation_params(&SX128X_DEV, LORA_SF_12, LORA_BW_200, LORA_CR_4_8);
    // NETSTACK_RADIO.on();

    // send message
    NETSTACK_RADIO.send(message, strlen(message));
}

// #endregion ------------------------------------------------------------------
// #region Process -------------------------------------------------------------
PROCESS(node_process, "Shell");
AUTOSTART_PROCESSES(&node_process);

PROCESS_THREAD(node_process, ev, data)
{
    static char buf[255];

    static struct etimer et;
    static int WAIT_TIME;
    static int SF;
    static int BW;
    static int j;
    static int k;
    static int i;
    PROCESS_BEGIN();
    static const int modes[6][3] = {
        {LORA_SF_7, LORA_BW_1600, 10}, // 0.06
        {LORA_SF_7, LORA_BW_200, 100}, // 0.5
        {LORA_SF_9, LORA_BW_1600, 30},
        {LORA_SF_9, LORA_BW_200, 150},
        {LORA_SF_12, LORA_BW_1600, 125},
        {LORA_SF_12, LORA_BW_200, 1000}};
    static const int cads_to_perform[6][5] = {
        {105, 85, 70, 53, 35},
        {195, 134, 86, 52, 30},
        {130, 95, 63, 40, 25},
        {180, 115, 70, 40, 20},
        {160, 110, 67, 37, 22},
        {181, 120, 70, 40, 23},
    };
    static const int arr_CAD_symbols[5] = {1, 2, 4, 8, 16};
    static const int arr_CAD_symbols_settings[5] = {CAD_SYMBOLS_01, CAD_SYMBOLS_02, CAD_SYMBOLS_04, CAD_SYMBOLS_08, CAD_SYMBOLS_16};

    // loop over every mode
    for (i = 0; i < 6; i++)
    {
        // set the correct parameters according to the mode
        SF = modes[i][0];
        printf("SF%d\n", convert_sf(SF));
        BW = modes[i][1];
        printf("BW%d\n", convert_bw(BW));
        WAIT_TIME = modes[i][2];

        // test every mode 5 times
        for (j = 0; j < 5; j++)
        {
            printf("CAD%d\n", arr_CAD_symbols[j]);
            // send start packet
            send_start_packet(i);

            // set timer
            etimer_set(&et, WAIT_TIME);

            // make sure the device is using the correct configurations
            // set CAD params
            sx128x_cmd_set_cad_params(&SX128X_DEV, arr_CAD_symbols_settings[j]);

            // repeat for 20 packets
            for (k = 0; k < AMOUNT_OF_PACKETS; k++)
            {
                // wait for timer to finish
                PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

                /* Reset the etimer to trig again in 1 second */
                etimer_reset(&et);

                // ! malloc not recommended, should be using memb_alloc
                int *results = (int *)malloc(cads_to_perform[i][j] * sizeof(int));
                // continuously perform CAD's
                for (int h = 0; h < cads_to_perform[i][j]; h++)
                {
                    results[h] = sx128x_channel_activity_detection();
                }
                for (int h = 0; h < cads_to_perform[i][j]; h++)
                {
                    printf("%d", results[h]);
                }
                // ! free not recommended, should be using memb_free
                free(results);
                printf("\n");
            }
        }

        // wait again for timer to allow for the transmitter to change mode
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        etimer_reset(&et);

        send_start_packet(i);

        etimer_reset(&et);

        // set the correct parameters
        sx128x_cmd_set_modulation_params(&SX128X_DEV, SF, BW, LORA_CR_4_5);
        NETSTACK_RADIO.on();

        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

        printf("Correct packet received:");
        for (k = 0; k < AMOUNT_OF_PACKETS; k++)
        {
            /* Reset the etimer to trig again in 1 second */
            etimer_reset(&et);

            NETSTACK_RADIO.on();

            while ((SX128X_DEV.state.event != SX128X_RX_DONE) && (etimer_expired(&et) != 1))
            {
                clock_delay_usec(50);
                watchdog_periodic();
            }
            NETSTACK_RADIO.read(buf, 255);
            if (strcmp(buf, message))
            {
                printf("1");
            }
            else
            {
                printf("0");
            }
        }
        printf("\n");
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        etimer_reset(&et);
    }
    PROCESS_END();
}
// #endregion ------------------------------------------------------------------