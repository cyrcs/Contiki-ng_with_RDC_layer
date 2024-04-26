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

// #endregion ------------------------------------------------------------------
// #region Process -------------------------------------------------------------
PROCESS(node_process, "Shell");
AUTOSTART_PROCESSES(&node_process);

PROCESS_THREAD(node_process, ev, data)
{

    PROCESS_BEGIN();
    NETSTACK_RADIO.on();
    sx128x_cmd_set_modulation_params(&SX128X_DEV, LORA_SF_5, LORA_BW_200, LORA_CR_4_5);
    sx128x_cmd_set_cad_params(&SX128X_DEV, CAD_SYMBOLS_08);
    while (1)
        printf("%d", sx128x_channel_activity_detection());
    PROCESS_END();
}

// #endregion ------------------------------------------------------------------