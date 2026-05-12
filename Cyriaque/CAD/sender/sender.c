// License ---------------------------------------------------------------------
/*
 * Copyright (c) 2020, Perale Thomas
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
/* -------------------------------------------------------------------------- */

/* ---------------------------- File Description ---------------------------- */
/**
 * \addtogroup zoul-examples
 * @{
 *
 * \defgroup zoul-sx1280-test RN2483 Wireless Module sensor.
 *
 * @{
 *
 * \filet
 *         A quick program for testing the SX1280 Wireless Module sensor.
 * \author
 *         Perale Thomas <tperale@vub.be>
 */
/* -------------------------------------------------------------------------- */

/* -------------------------------- Libraries ------------------------------- */
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
/* -------------------------------------------------------------------------- */

/* --------------------------------- Defines -------------------------------- */
#define LOG_MODULE "MAIN"
#define LOG_LEVEL LOG_LEVEL_DBG
#define AMOUNT_OF_PACKETS 10
static int i;
static char buf[255];
static char message[256] = "helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld aa\0";
static struct etimer et;

static const int modes[6][3] = {
    {LORA_SF_7, LORA_BW_1600, 10}, // 0.06
    {LORA_SF_7, LORA_BW_200, 100}, // 0.5
    {LORA_SF_9, LORA_BW_1600, 30},
    {LORA_SF_9, LORA_BW_200, 150},
    {LORA_SF_12, LORA_BW_1600, 125},
    {LORA_SF_12, LORA_BW_200, 1000}};
/* -------------------------------------------------------------------------- */
void wait_for_start_packet()
{
    printf("waiting for start packet\n");
    // wait for start packet
    int start_packet_found = 0;
    while (!start_packet_found)
    {
        // turn on radio and configure it to receive start packet
        NETSTACK_RADIO.on();
        sx128x_cmd_set_modulation_params(&SX128X_DEV, LORA_SF_12, LORA_BW_200, LORA_CR_4_8);

        // wait to receive a packet
        while (SX128X_DEV.state.event != SX128X_RX_DONE)
        {
            clock_delay_usec(50);
            watchdog_periodic();
        }

        // when a packet is received, read it
        NETSTACK_RADIO.read(buf, 255);

        // check if packet is a start packet and configure correctly, else ignore and continue waiting
        if (strcmp(buf, "start: 0") == 0)
        {
            printf("setting mode 0\n");
            start_packet_found = 1;
            sx128x_cmd_set_modulation_params(&SX128X_DEV, modes[0][0], modes[0][1], LORA_CR_4_5);
            etimer_set(&et, modes[0][2]);
        }
        else if (strcmp(buf, "start: 1") == 0)
        {
            printf("setting mode 1\n");
            start_packet_found = 1;
            sx128x_cmd_set_modulation_params(&SX128X_DEV, modes[1][0], modes[1][1], LORA_CR_4_5);
            etimer_set(&et, modes[1][2]);
        }
        else if (strcmp(buf, "start: 2") == 0)
        {
            printf("setting mode 2\n");
            start_packet_found = 1;
            sx128x_cmd_set_modulation_params(&SX128X_DEV, modes[2][0], modes[2][1], LORA_CR_4_5);
            etimer_set(&et, modes[2][2]);
        }
        else if (strcmp(buf, "start: 3") == 0)
        {
            printf("setting mode 3\n");
            start_packet_found = 1;
            sx128x_cmd_set_modulation_params(&SX128X_DEV, modes[3][0], modes[3][1], LORA_CR_4_5);
            etimer_set(&et, modes[3][2]);
        }
        else if (strcmp(buf, "start: 4") == 0)
        {
            for (int t = 0; t < 50; t++)
            {
                clock_delay_usec(1000);
                watchdog_periodic();
            }
            printf("setting mode 4\n");
            start_packet_found = 1;
            sx128x_cmd_set_modulation_params(&SX128X_DEV, modes[4][0], modes[4][1], LORA_CR_4_5);
            etimer_set(&et, modes[4][2]);
        }
        else if (strcmp(buf, "start: 5") == 0)
        {
            for (int t = 0; t < 500; t++)
            {
                clock_delay_usec(1000);
                watchdog_periodic();
            }
            printf("setting mode 5\n");
            start_packet_found = 1;
            sx128x_cmd_set_modulation_params(&SX128X_DEV, modes[5][0], modes[5][1], LORA_CR_4_5);
            etimer_set(&et, modes[5][2]);
        }
    }
}
/* ------------------------------ Process test ------------------------------ */
PROCESS(node_process, "Shell");
AUTOSTART_PROCESSES(&node_process);

PROCESS_THREAD(node_process, ev, data)
{
    PROCESS_BEGIN();

    while (1)
    {
        // wait for start packet
        wait_for_start_packet();

        // send 20 packets
        i = 0;
        while (i < AMOUNT_OF_PACKETS)
        {
            // wait for timer to finish
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

            /* Reset the etimer to trig again in 1 second */
            etimer_reset(&et);

            // send packet
            LOG_DBG("Broadcasting raw data packet!\n");
            NETSTACK_RADIO.send(message, strlen(message));
            printf("packet length: %d\n", strlen(message));
            i++;
        }
    }
    PROCESS_END();
}
/* -------------------------------------------------------------------------- */