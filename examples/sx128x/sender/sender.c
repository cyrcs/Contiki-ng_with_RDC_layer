// #region License -------------------------------------------------------------
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
// #endregion ------------------------------------------------------------------
// #region File Description ----------------------------------------------------
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
// #endregion ------------------------------------------------------------------
// #region Libraries------------------------------------------------------------
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
char test[] = "helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld helloworld ";
char test2[] = "helloworld_helloworld";
int value = 0;
// #endregion ------------------------------------------------------------------
// #region Process -------------------------------------------------------------
PROCESS(sender_process, "Shell");
AUTOSTART_PROCESSES(&sender_process);
//printf("check 1");
PROCESS_THREAD(sender_process, ev, data)
{
  static struct etimer et;

  PROCESS_BEGIN();

  printf("check 2\n");
  LOG_DBG("process started\n");

  etimer_set(&et, CLOCK_SECOND * 2);

  while(1) {

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    etimer_reset(&et);

    LOG_DBG("trying to send\n");

    value++;
    if(value > 999) value = 0;

    char test3[8];
    sprintf(test3, "%d", value);

    char result[64];
    strcpy(result, test2);
    strcat(result, " ");
    strcat(result, test3);

    NETSTACK_RADIO.send(result, strlen(result));
  }

  PROCESS_END();
}
// #endregion ------------------------------------------------------------------
