#include "sx128x.h"
#include "contiki.h"
#include "dev/gpio-hal.h"
#include "dev/leds.h"
#include "ioc.h"
#include "net/mac/tsch/tsch.h"
#include "os/net/packetbuf.h"
#include "radio.h"
#include "rtimer-arch.h"
#include "rtimer.h"
#include "spi.h"
#include "sys/_stdint.h"
#include "watchdog.h"
#include <stdint.h>

#include "sys/log.h"
#define LOG_MODULE "SX128X"
#define LOG_LEVEL LOG_CONF_LEVEL_RADIO

PROCESS(sx128x_rf_process, "sx128x RF driver");
static int sx128x_on(void);

// static uint8_t volatile poll_mode = 0;

/* ---------------------------- CONFIGURE DEVICE ---------------------------- */

sx128x_t __sx128x_dev = {
    .state = {},
    .settings = {
        .channel = SX128X_CHANNEL_DEFAULT,
        .lora = {},
    },
    .params = {.spi = {
                   .spi_controller = SX128X_SPI_CONTROLLER,
                   .pin_spi_sck = GPIO_PORT_PIN_TO_GPIO_HAL_PIN(SX128X_SPI_SCK_PORT, SX128X_SPI_SCK),
                   .pin_spi_miso = GPIO_PORT_PIN_TO_GPIO_HAL_PIN(SX128X_SPI_MISO_PORT, SX128X_SPI_MISO),
                   .pin_spi_mosi = GPIO_PORT_PIN_TO_GPIO_HAL_PIN(SX128X_SPI_MOSI_PORT, SX128X_SPI_MOSI),
                   .pin_spi_cs = GPIO_PORT_PIN_TO_GPIO_HAL_PIN(SX128X_SPI_CS_PORT, SX128X_SPI_CS),
                   .spi_bit_rate = SX128X_SPI_BITRATE,
                   .spi_pha = SX128X_SPI_PHASE,
                   .spi_pol = SX128X_SPI_POL,
               },
               .reset_pin = GPIO_PORT_PIN_TO_GPIO_HAL_PIN(SX128X_RESET_GPIO_PORT, SX128X_RESET_GPIO),
               .busy_pin = GPIO_PORT_PIN_TO_GPIO_HAL_PIN(SX128X_BUSY_PORT, SX128X_BUSY_PIN),
               .dio1_pin = GPIO_PORT_PIN_TO_GPIO_HAL_PIN(SX128X_DIO1_PORT, SX128X_DIO1_PIN)},
    ._internal = {},
    .irq = 0,
};
/* -------------------------------- INTERRUPT ------------------------------- */
#if SX128X_USE_INTERRUPT
#define SX128X_DIO1_PORT_BASE GPIO_PORT_TO_BASE(SX128X_DIO1_PORT)
#define SX128X_DIO1_PIN_MASK GPIO_PIN_MASK(SX128X_DIO1_PIN)

void sx128x_interrupt_opmode_receiver()
{
    LOG_DBG("OPMODE RECEIVER\n");

    switch (SX128X_DEV.irq)
    {
    case SX128X_IRQ_REG_RX_DONE:
        LOG_INFO("Flag set: RX DONE\n");
        SX128X_DEV._internal.rx_timestamp = RTIMER_NOW();
        sx128x_cmd_get_packet_status(&SX128X_DEV);
        sx128x_cmd_get_rx_buffer_status(&SX128X_DEV);
        sx128x_set_state_rx(&SX128X_DEV, SX128X_RX_RECEIVED);
        sx128x_set_state_event(&SX128X_DEV, SX128X_RX_DONE);
        process_poll(&sx128x_rf_process);
        break;
    case SX128X_IRQ_REG_RX_TX_TIMEOUT:
        LOG_INFO("Flag set: RX TIMEOUT\n");
        sx128x_set_standby(&SX128X_DEV);
        sx128x_set_state_event(&SX128X_DEV, SX128X_RX_TIMEOUT);
        sx128x_set_state_rx(&SX128X_DEV, SX128X_RX_OFF);
        process_poll(&sx128x_rf_process);
        break;
    case SX128X_IRQ_REG_CRC_ERROR:
        LOG_INFO("Flag set: CRC ERROR\n");
        break;
    }
}

void sx128x_interrupt_opmode_receiver_ack()
{
    LOG_DBG("OPMODE RECEIVER\n");

    switch (SX128X_DEV.irq)
    {
    case SX128X_IRQ_REG_RX_DONE:
        LOG_INFO("Flag: RX DONE\n");
        SX128X_DEV._internal.rx_timestamp = RTIMER_NOW();
        sx128x_set_standby(&SX128X_DEV);
        sx128x_cmd_get_packet_status(&SX128X_DEV);
        sx128x_cmd_get_rx_buffer_status(&SX128X_DEV);
        sx128x_set_state_event(&SX128X_DEV, SX128X_RX_DONE);

        process_poll(&sx128x_rf_process);
        break;
    case SX128X_IRQ_REG_RX_TX_TIMEOUT:
        LOG_INFO("Flag: RX TIMEOUT\n");
        sx128x_set_standby(&SX128X_DEV);
        sx128x_set_state_event(&SX128X_DEV, SX128X_RX_TIMEOUT);
        sx128x_set_state_rx(&SX128X_DEV, SX128X_RX_OFF);
        process_poll(&sx128x_rf_process);
        break;
    }
}

void sx128x_interrupt_opmode_receiver_continuous()
{
    LOG_WARN("OPMODE RECEIVER_CONTINUOUS: NOT IMPLEMENTED!!!!!!!!!\n");
}

void sx128x_interrupt_opmode_cad()
{
    LOG_DBG("OPMODE CAD\n");

    switch (SX128X_DEV.irq)
    {
    case SX128X_IRQ_REG_CAD_DETECTED | SX128X_IRQ_REG_CAD_DONE: // ! CAD done is always triggered when CAD detected is so we need a case for both flags.
        LOG_INFO("Flag set: CAD DETECTED, took %llu ticks\n", (RTIMER_NOW()) - SX128X_DEV._internal.cad_timestamp);

        sx128x_set_state_event(&SX128X_DEV, SX128X_CAD_DETECTED);
        break;
    case SX128X_IRQ_REG_CAD_DONE:
        // sx128x_set_standby(&SX128X_DEV);
        LOG_INFO("Flag set: CAD DONE, took %llu ticks\n", (RTIMER_NOW()) - SX128X_DEV._internal.cad_timestamp);

        sx128x_set_state_event(&SX128X_DEV, SX128X_CAD_DONE);
        break;
    }
}

void sx128x_interrupt_opmode_transmitter()
{
    LOG_DBG("OPMODE TRANSMITTER\n");
    switch (SX128X_DEV.irq)
    {
    case SX128X_IRQ_REG_TX_DONE:
        sx128x_set_state_event(&SX128X_DEV, SX128X_TX_DONE);
        LOG_INFO("Flag set: TX DONE, took %llu ticks\n", (RTIMER_NOW()) - SX128X_DEV._internal.tx_timestamp);
        break;
    case SX128X_IRQ_REG_RX_TX_TIMEOUT:
        sx128x_set_state_event(&SX128X_DEV, SX128X_TX_TIMEOUT);
        LOG_INFO("Flag set: TX TIMEOUT\n");
        break;
    default:
        LOG_ERR("TX IRQ NOT FOUND: %d\n", SX128X_DEV.irq);
    }
}

static void sx128x_interrupt_dio1(gpio_hal_pin_mask_t pin_mask)
{
    LOG_INFO("Interrupt on DIO1\n");
    LOG_FUNC("Function call: %s\n", __func__);
    // printf("inside interrupt\n");
    // get irq status and then reset it
    SX128X_DEV.irq = sx128x_cmd_get_irq_status(&SX128X_DEV);
    sx128x_cmd_clear_irq_status(&SX128X_DEV, SX128X_IRQ_REG_ALL);
    // get current opmode
    uint8_t current_opmode = sx128x_get_state_opmode(&SX128X_DEV);
    LOG_DBG("opmode %d with IRQ: %d\n", current_opmode, SX128X_DEV.irq);

    // if all flags are 0, just return
    if (SX128X_DEV.irq == 0)
        return;

    switch (current_opmode)
    {
    case SX128X_OPMODE_CAD:
        sx128x_interrupt_opmode_cad();
        break;
    case SX128X_OPMODE_RX:
        sx128x_interrupt_opmode_receiver();
        break;
    case SX128X_OPMODE_RX_SINGLE:
        sx128x_interrupt_opmode_receiver();
        break;
    case SX128X_OPMODE_RX_ACK:
        sx128x_interrupt_opmode_receiver_ack();
        break;
    case SX128X_OPMODE_RX_CONTINUOUS:
        sx128x_interrupt_opmode_receiver_continuous();
        break;
    case SX128X_OPMODE_TX:
        sx128x_interrupt_opmode_transmitter();
        break;
    default:
        LOG_ERR("OPMODE NOT FOUND: %d\n", current_opmode);
        break;
    }
    LOG_DBG("Done handling interrupt\n");
    // printf("done interrupt\n");
}

gpio_hal_event_handler_t sx128x_event_handler_dio1 = {
    .next = NULL,
    .handler = sx128x_interrupt_dio1,
    .pin_mask = (gpio_hal_pin_to_mask(SX128X_DIO1_PIN) << (SX128X_DIO1_PORT << 3))};
#endif // SX128X_USE_INTERRUPT

/* ----------------------------------- TX ----------------------------------- */
static int
sx128x_prepare(const void *payload, unsigned short payload_len)
{
    LOG_FUNC("Function call: %s\n", __func__);
    LOG_DBG("payload(%d): %s\n", payload_len, (char *)payload);

    // ! should we check is or just always go to standby mode?
    // if currently not in standby, set in standby
    if (sx128x_get_state_opmode(&SX128X_DEV) != SX128X_OPMODE_STANDBY)
    {
        sx128x_set_standby(&SX128X_DEV);
    }
    // reset address pointer
    sx128x_cmd_set_buffer_base_address(&SX128X_DEV, 0, 0);
    // configure Tx mode
    sx128x_cmd_set_tx_params(&SX128X_DEV, 13, 0xE0);
    // write payload to fifo
    sx128x_set_payload_length(&SX128X_DEV, payload_len);
    sx128x_write_fifo(&SX128X_DEV, (uint8_t *)payload, payload_len);

    return RADIO_RESULT_OK;
}

static int
sx128x_transmit(unsigned short payload_len)
{
    LOG_FUNC("Function call: %s\n", __func__);

    sx128x_set_state_opmode(&SX128X_DEV, SX128X_OPMODE_TX);

    unsigned int ticks = 0;
    unsigned int timeoutValue = 100000; // in 100us => 10s
    while ((sx128x_get_state_event(&SX128X_DEV) != SX128X_TX_DONE) && (ticks < timeoutValue))
    {
        watchdog_periodic();
        clock_delay_usec(100);
        ticks++;
    }
    LOG_DBG("Transmitted after %d ticks\n", ticks);
    // this part could be put in the process poll handler instead of here.
    if (sx128x_get_state_event(&SX128X_DEV) == SX128X_TX_DONE)
    {

        LOG_DBG("Transmitted %d bytes with success\n", payload_len);
        // if (payload_len == 44)
        // {
        //     sx128x_set_state_opmode(&SX128X_DEV, SX128X_OPMODE_RX_SINGLE);
        // }
        if (payload_len != 3)
        {
#if CSMA_CONF_SEND_SOFT_ACK
            sx128x_set_state_opmode(&SX128X_DEV, SX128X_OPMODE_RX_ACK);
#endif
        }
        else
        {

            sx128x_set_standby(&SX128X_DEV);
        }
        return RADIO_TX_OK;
    }
    else if (sx128x_get_state_event(&SX128X_DEV) == SX128X_TX_TIMEOUT)
    {
        LOG_DBG("Failed to transmit %d bytes\n", payload_len);
        sx128x_set_standby(&SX128X_DEV);
        return RADIO_TX_ERR;
    }
    else
    {
        LOG_ERR("ERROR: INTERRUPT PIN NOT TRIGGERED\n");
        sx128x_set_standby(&SX128X_DEV);
        return RADIO_TX_ERR;
    }
}

static int
sx128x_send(const void *payload, unsigned short payload_len)
{
    LOG_FUNC("Function call: %s\n", __func__);
    sx128x_prepare(payload, payload_len);
    return sx128x_transmit(payload_len);
}

/* ----------------------------------- RX ----------------------------------- */
static int
sx128x_pending_packet(void)
{
    LOG_FUNC("Function call: %s\n", __func__);
    if (SX128X_DEV.state.event == SX128X_RX_DONE)
    {
        return true;
    }

    return false;

    // if (SX128X_DEV.state.rx == sx128x_rx_received) { // this is set in the interrupt function
    //   return true;
    // } else if (SX128X_DEV.state.rx == sx128x_rx_listening) {
    //   sx128x_receiving_packet();
    //   return false;
    // }
}

static int
sx128x_receiving_packet(void)
{
    LOG_FUNC("Function call: %s\n", __func__);

    // ! this setting is only set to receiving when using CAD, otherwise it goes from off to listening to received
    if (SX128X_DEV.state.rx == SX128X_RX_RECEIVING)
    {
        if (sx128x_pending_packet())
        {
            return false;
        }
        return true;
    }
    return SX128X_DEV.state.rx == SX128X_RX_RECEIVING;
}

static int
sx128x_read_packet(void *buf, unsigned short bufsize)
{
    LOG_FUNC("Function call: %s\n", __func__);

    if (!sx128x_pending_packet())
    {
        LOG_DBG("No packet pending...\n");
        return 0;
    }
    LOG_DBG("succeeded reading packet\n");

    // TODO length and SNR should have already been fetched from the pending fn
    // TODO Make sure everything is OK in anycase
    sx128x_read_fifo(&SX128X_DEV, buf, SX128X_DEV._internal.rx_length < bufsize ? SX128X_DEV._internal.rx_length : bufsize);
    if (SX128X_DEV._internal.rx_length < bufsize)
    {
        ((uint8_t *)buf)[SX128X_DEV._internal.rx_length] = '\0';
        LOG_DBG("size error\n");
    }

    LOG_INFO("Received packet of %d bytes\n", SX128X_DEV._internal.rx_length);

    return SX128X_DEV._internal.rx_length;
}

static void sx128x_poll_handler(void)
{
    LOG_FUNC("Function call: %s\n", __func__);
    int len;

    if (sx128x_pending_packet() && sx128x_get_state_rx(&SX128X_DEV) == SX128X_RX_RECEIVED)
    {
        LOG_INFO("reading packet\n");
        packetbuf_clear();
        len = sx128x_read_packet(packetbuf_dataptr(), (&SX128X_DEV)->_internal.rx_length);
        if (len > 0)
        {
            packetbuf_set_datalen(len);
            NETSTACK_RDC.input();
        }
        // ! hack for continuous mode while using single mode
        if ((&SX128X_DEV)->settings.rx_mode == SX128X_RX_MODE_CONTINUOUS)
        {
            LOG_DBG("Restarting RX\n");
            sx128x_on();
        }
    }
    else if (sx128x_get_state_event(&SX128X_DEV) == SX128X_RX_TIMEOUT)
    {
        LOG_DBG("processed RX TIMEOUT\n");
    }
}

PROCESS_THREAD(sx128x_rf_process, ev, data)
{
    LOG_FUNC("Function call: %s\n", __func__);

    PROCESS_POLLHANDLER(sx128x_poll_handler());

    PROCESS_BEGIN();

    PROCESS_YIELD_UNTIL(ev == PROCESS_EVENT_EXIT);

    PROCESS_END();
}

/* ----------------------------------- CAD ---------------------------------- */
int sx128x_clear_channel_assesment()
{
    LOG_FUNC("Function call: %s\n", __func__);

    return CCA_CLEAR;
}

int sx128x_channel_activity_detection()
{
    LOG_FUNC("Function call: %s\n", __func__);

    // do a CAD
    sx128x_set_cad(&SX128X_DEV);

    // wait for interrupt for either CAD done or CAD detected
    while ((sx128x_get_state_event(&SX128X_DEV) != SX128X_CAD_DONE) && (sx128x_get_state_event(&SX128X_DEV) != SX128X_CAD_DETECTED))
    {
        clock_delay_usec(50);
        watchdog_periodic();
    }

    if (sx128x_get_state_event(&SX128X_DEV) == SX128X_CAD_DETECTED)
    {
        LOG_DBG("CAD DETECTED\n");
        return CCA_BUSY;
    }
    else if (sx128x_get_state_event(&SX128X_DEV) == SX128X_CAD_DONE)
    {
        LOG_DBG("CAD DONE, NO MESSAGE\n");
        return CCA_CLEAR;
    }
    else
    {
        LOG_DBG("CAD ERROR\n");
        return CCA_ERROR;
    }
}
/* --------------------------------- ON/OFF --------------------------------- */
static int
sx128x_on(void)
{
    LOG_FUNC("Function call: %s\n", __func__);
#if !SX128X_BUSY_RX
    // sx128x_cmd_set_dio_irq_params(&(SX128X_DEV), SX128X_IRQ_REG_TX_DONE | SX128X_IRQ_REG_RX_DONE | SX128X_IRQ_REG_CAD_DONE | SX128X_IRQ_REG_CAD_DETECTED | SX128X_IRQ_REG_RX_TX_TIMEOUT, 0, 0);
    sx128x_set_state_opmode(&SX128X_DEV, SX128X_OPMODE_RX_SINGLE);
#endif
    return 1;
}

static int
sx128x_off(void)
{
    LOG_FUNC("Function call: %s\n", __func__);
    sx128x_set_state_opmode(&SX128X_DEV, SX128X_OPMODE_STANDBY);
    sx128x_cmd_clear_irq_status(&SX128X_DEV, SX128X_IRQ_REG_ALL);
    return 1;
}

/* --------------------------------- GET/SET -------------------------------- */
// ! UNTESTED, THIS NEEDS TO BE UPDATED, LIST IN RADIO.H
radio_result_t sx128x_get_value(radio_param_t param, radio_value_t *value)
{
    if (!value)
    {
        return RADIO_RESULT_INVALID_VALUE;
    }

    switch (param)
    {
    case RADIO_PARAM_POWER_MODE:
        *value = SX128X_DEV.state.opmode == SX128X_OPMODE_STANDBY ? RADIO_POWER_MODE_OFF : RADIO_POWER_MODE_ON;
        return RADIO_RESULT_OK;
    case RADIO_PARAM_CHANNEL:
        switch (sx128x_get_frequency(&SX128X_DEV))
        {
        case 2400:
            *value = 0;
            break;
        }
        return RADIO_RESULT_OK;
    // case RADIO_PARAM_RX_MODE:
    //   if (!sx128x_get_rx_single(&SX128X_DEV)) {
    //     *value |= RADIO_RX_MODE_POLL_MODE;
    //   }
    //  if(SX128X_DEV.settings.lora.flags & SX128X_RX_CONTINUOUS_FLAG) {
    //  }
    //  if(SX128X_DEV.settings.lora.rx_auto_ack) {
    //    *value |= RADIO_RX_MODE_AUTOACK;
    //  }
    //  if(SX128X_DEV.settings.lora.rx_address_filter) {
    //    *value |= RADIO_RX_MODE_ADDRESS_FILTER;
    //  }

    //   return RADIO_RESULT_OK;
    case RADIO_PARAM_TX_MODE:
        /* if(SX128X_DEV.settings.lora.tx_cca) { */
        /*   *value |= RADIO_TX_MODE_SEND_ON_CCA; */
        /* } */
        return RADIO_RESULT_OK;
    case RADIO_PARAM_TXPOWER:
        *value = SX128X_DEV.settings.power;
        return RADIO_RESULT_OK;
    case RADIO_PARAM_CCA_THRESHOLD:
        /*
         * Clear channel assessment threshold in dBm. This threshold
         * determines the minimum RSSI level at which the radio will assume
         * that there is a packet in the air.
         */
        return RADIO_RESULT_NOT_SUPPORTED;
    case RADIO_PARAM_RSSI:
        /* Return the RSSI value in dBm */
        return RADIO_RESULT_NOT_SUPPORTED;
    case RADIO_PARAM_LAST_RSSI:
        /* RSSI of the last packet received */
        *value = SX128X_DEV._internal.rx_rssi;
        return RADIO_RESULT_OK;
    case RADIO_PARAM_LAST_LINK_QUALITY:
        /* LQI of the last packet received */
        *value = SX128X_DEV._internal.rx_snr;
        return RADIO_RESULT_OK;
    case RADIO_CONST_CHANNEL_MIN:
        *value = 0;
        return RADIO_RESULT_OK;
    case RADIO_CONST_CHANNEL_MAX:
        *value = 2;
        return RADIO_RESULT_OK;
    case RADIO_CONST_TXPOWER_MIN:
        *value = 0;
        return RADIO_RESULT_OK;
    case RADIO_CONST_TXPOWER_MAX:
        *value = 14;
        return RADIO_RESULT_OK;
    case RADIO_CONST_MAX_PAYLOAD_LEN:
        *value = (radio_value_t)255;
        return RADIO_RESULT_OK;
    case RADIO_CONST_PHY_OVERHEAD:
        *value = sx128x_get_preamble_length(&SX128X_DEV) + (sx128x_get_crc(&SX128X_DEV) ? 5 : 0);
        return RADIO_RESULT_OK;
    case RADIO_CONST_BYTE_AIR_TIME:
        *value = 0;
        return RADIO_RESULT_OK;
    case RADIO_CONST_DELAY_BEFORE_TX:
        *value = 0;
        switch (SX128X_SPI_BITRATE)
        {
        case 8000000:
            *value = US_TO_RTIMERTICKS(60    // internal time documented in datasheet
                                       + 122 // time  to switch from standby mode to transmit mode
                                       + 80);
            break;
        default:
            LOG_ERR("Bitrate not supported\n");
        }
        return RADIO_RESULT_OK;
    case RADIO_CONST_DELAY_BEFORE_RX:
        *value = 0;
        switch (SX128X_SPI_BITRATE)
        {
        case 8000000:
            *value = US_TO_RTIMERTICKS(
                71 + 153 // Time to set op_mode CAD
            );
            break;
        default:
            LOG_ERR("Bitrate not supported\n");
        }
        return RADIO_RESULT_OK;
    case RADIO_CONST_DELAY_BEFORE_DETECT:
        *value = 0; // US_TO_RTIMERTICKS(2 * t_sym(SX128X_DEV.settings.lora.sf, SX128X_DEV.settings.lora.bw));
        return RADIO_RESULT_OK;
    default:
        return RADIO_RESULT_NOT_SUPPORTED;
    }

    return RADIO_RESULT_OK;
}

// ! UNTESTED, THIS NEEDS TO BE UPDATED, LIST IN RADIO.H
radio_result_t sx128x_set_value(radio_param_t param, radio_value_t value)
{
    LOG_FUNC("Function call: %s\n", __func__);
    switch (param)
    {
    case RADIO_PARAM_POWER_MODE:
        if (value == RADIO_POWER_MODE_ON)
        {
            sx128x_on();
            return RADIO_RESULT_OK;
        }
        if (value == RADIO_POWER_MODE_OFF)
        {
            sx128x_off();
            return RADIO_RESULT_OK;
        }
        if (value == RADIO_POWER_MODE_CARRIER_ON ||
            value == RADIO_POWER_MODE_CARRIER_OFF)
        {
            return RADIO_RESULT_NOT_SUPPORTED;
        }
        return RADIO_RESULT_INVALID_VALUE;
    case RADIO_PARAM_CHANNEL:
        sx128x_set_state_opmode(&SX128X_DEV, SX128X_OPMODE_SLEEP);
        sx128x_cmd_set_frequency(&SX128X_DEV, 2400);
        sx128x_set_state_opmode(&SX128X_DEV, SX128X_OPMODE_STANDBY);
        return RADIO_RESULT_OK;
    case RADIO_PARAM_RX_MODE:
        return RADIO_RESULT_OK;
        if (value & ~(RADIO_RX_MODE_ADDRESS_FILTER | RADIO_RX_MODE_AUTOACK | RADIO_RX_MODE_POLL_MODE))
        {
            return RADIO_RESULT_INVALID_VALUE;
        }
        /* RN2483_DEV.radio.rx_continuous = (value & RADIO_RX_MODE_POLL_MODE) != 0; */
        /* RN2483_DEV.radio.rx_auto_ack = (value & RADIO_RX_MODE_AUTOACK) != 0; */
        /* RN2483_DEV.radio.rx_address_filter = (value & RADIO_RX_MODE_ADDRESS_FILTER) != 0; */
        return RADIO_RESULT_OK;
    case RADIO_PARAM_TX_MODE:
        /* RN2483_DEV.radio.tx_cca = (value & RADIO_TX_MODE_SEND_ON_CCA) != 0; */
        return RADIO_RESULT_OK;
    // case RADIO_PARAM_TXPOWER:
    //   // TODO verification
    //   /* if(value < RADIO_PWR_MINUS_3 || value > RADIO_PWR_15) { */
    //   /*   return RADIO_RESULT_INVALID_VALUE; */
    //   /* } */
    //   /* Find the closest higher PA_LEVEL for the desired output power */
    //   sx128x_set_tx_power(&SX128X_DEV, value);
    //   return RADIO_RESULT_OK;
    case RADIO_PARAM_CCA_THRESHOLD:
        /*
         * Clear channel assessment threshold in dBm. This threshold
         * determines the minimum RSSI level at which the radio will assume
         * that there is a packet in the air.
         *
         * The CCA threshold must be set to a level above the noise floor of
         * the deployment. Otherwise mechanisms such as send-on-CCA and
         * low-power-listening duty cycling protocols may not work
         * correctly. Hence, the default value of the system may not be
         * optimal for any given deployment.
         */
        return RADIO_RESULT_NOT_SUPPORTED;
    default:
        return RADIO_RESULT_NOT_SUPPORTED;
    }
    return RADIO_RESULT_OK;
}

// ! UNTESTED, THIS NEEDS TO BE UPDATED, LIST IN RADIO.H
radio_result_t sx128x_get_object(radio_param_t param, void *dest, size_t size)
{
    LOG_FUNC("Function call: %s\n", __func__);
    switch (param)
    {
    case RADIO_PARAM_LAST_PACKET_TIMESTAMP:
        if (size != sizeof(rtimer_clock_t) || !dest)
        {
            return RADIO_RESULT_INVALID_VALUE;
        }
        /* LOG_DBG("LORA COMM -> %d us of length %d bytes\n", t_packet(&(SX128X_DEV.settings.lora), SX128X_DEV._internal.rx_length), SX128X_DEV._internal.rx_length); */
        /* *(rtimer_clock_t *)dest = SX128X_DEV._internal.rx_timestamp - US_TO_RTIMERTICKS( */
        /*   t_packet(&(SX128X_DEV.settings.lora), SX128X_DEV.settings.rx_length) */
        /*   + 622 // Delay between TX end of transmission and RX detection of end of transmission */
        /*   + 152 // Delay between interrupt on DIO1 and software detection */
        /* ); */
        return RADIO_RESULT_OK;
    }

    return RADIO_RESULT_OK;
}

// ! UNTESTED, THIS NEEDS TO BE UPDATED, LIST IN RADIO.H
radio_result_t sx128x_set_object(radio_param_t param, const void *src, size_t size)
{
    LOG_FUNC("Function call: %s\n", __func__);
    return RADIO_RESULT_OK;
}
/* -------------------------------------------------------------------------- */
int sx128x_reset(const sx128x_t *dev)
{
    LOG_FUNC("Function call: %s\n", __func__);
    gpio_hal_arch_pin_set_output(0, dev->params.reset_pin);
    gpio_hal_arch_set_pin(0, dev->params.reset_pin);
    clock_delay_usec(20000);
    gpio_hal_arch_clear_pin(0, dev->params.reset_pin);
    clock_delay_usec(50000);
    gpio_hal_arch_set_pin(0, dev->params.reset_pin);
    gpio_hal_arch_pin_set_input(0, dev->params.reset_pin);
    clock_delay_usec(20000);

    return 0;
}

/* ----------------------------- INITIALIZATION ----------------------------- */
static void sx128x_gpio_init(sx128x_t *dev)
{
    LOG_FUNC("Function call: %s\n", __func__);
    gpio_hal_arch_no_port_pin_cfg_set(dev->params.dio1_pin, GPIO_HAL_PIN_CFG_PULL_DOWN);
    gpio_hal_arch_pin_set_input(0, dev->params.dio1_pin);

/*---------------------------------------------------------------------------*/
#if !SX128X_BUSY_RX
    GPIO_SOFTWARE_CONTROL(SX128X_DIO1_PORT_BASE, SX128X_DIO1_PIN_MASK);
    GPIO_SET_INPUT(SX128X_DIO1_PORT_BASE, SX128X_DIO1_PIN_MASK);
    GPIO_DETECT_EDGE(SX128X_DIO1_PORT_BASE, SX128X_DIO1_PIN_MASK);
    GPIO_DETECT_RISING(SX128X_DIO1_PORT_BASE, SX128X_DIO1_PIN_MASK);
    GPIO_TRIGGER_SINGLE_EDGE(SX128X_DIO1_PORT_BASE, SX128X_DIO1_PIN_MASK);
    ioc_set_over(SX128X_DIO1_PORT, SX128X_DIO1_PIN, IOC_OVERRIDE_DIS);
    gpio_hal_register_handler(&sx128x_event_handler_dio1);
    GPIO_ENABLE_INTERRUPT(SX128X_DIO1_PORT_BASE, SX128X_DIO1_PIN_MASK);
    NVIC_EnableIRQ(GPIO_B_IRQn);
#endif
}

static void sx128x_init_radio(sx128x_t *dev)
{
    LOG_FUNC("Function call: %s\n", __func__);
    LOG_DBG("cmd set regulator mode\n");
    sx128x_cmd_set_regulator_mode(dev, SX128X_REGULATOR_MODE_DCDC);
    LOG_DBG("cmd set packet type\n");
    sx128x_cmd_set_packet_type(dev, SX128X_PACKET_TYPE_DEFAULT);
    LOG_DBG("cmd set standby\n");
    sx128x_set_standby(dev);
    LOG_DBG("set default irq mask\n");
    sx128x_cmd_set_dio_irq_params(dev, SX128X_IRQ_REG_TX_DONE | SX128X_IRQ_REG_RX_DONE | SX128X_IRQ_REG_CAD_DONE | SX128X_IRQ_REG_CAD_DETECTED | SX128X_IRQ_REG_RX_TX_TIMEOUT, 0, 0);

    // LoRa settings
    LOG_DBG("Configuring LoRa modulation\n");
    sx128x_set_spreading_factor(dev, CONFIG_LORA24_SF_DEFAULT);
    sx128x_set_bandwidth(dev, CONFIG_LORA24_BW_DEFAULT);
    sx128x_set_coding_rate(dev, CONFIG_LORA24_CR_DEFAULT);

    sx128x_cmd_set_cad_params(&SX128X_DEV, CONFIG_LORA24_CAD_SYMBOLS_DEFAULT);

    // default packet parameters
    sx128x_set_preamble_length(dev, CONFIG_LORA24_PREAMBLE_LENGTH_DEFAULT);
    sx128x_set_fixed_header_len_mode(dev, CONFIG_LORA24_FIXED_HEADER_LEN_MODE_DEFAULT);
    sx128x_set_iq_inverted(dev, CONFIG_LORA24_INVERTED_IQ_DEFAULT);
    sx128x_set_crc(dev, CONFIG_LORA24_PAYLOAD_CRC_ON_DEFAULT);

    sx128x_cmd_set_frequency(dev, SX128X_CHANNEL_DEFAULT);
    sx128x_cmd_set_buffer_base_address(dev, 0, 0);

    dev->settings.rx_mode = SX128X_RX_MODE_CONTINUOUS;
}

int sx128x_initialization()
{
    LOG_FUNC("Function call: %s\n", __func__);
    if (spi_acquire(&SX128X_DEV.params.spi) != SPI_DEV_STATUS_OK)
    {
        LOG_ERR("Error init SPI\n");
        LOG_ERR("%d\n", spi_acquire(&SX128X_DEV.params.spi));
        return RADIO_RESULT_ERROR;
    }
    sx128x_gpio_init(&SX128X_DEV);
    sx128x_init_radio(&SX128X_DEV);

    process_start(&sx128x_rf_process, NULL);

    LOG_DBG("AFTER INIT RADIO\n");

    LOG_INFO("Initialized LoRa module with SF: %d, CR: %d, BW: %d, CRC: %d, PRLEN: %d, HEADER: %d\n",
             sx128x_get_spreading_factor(&SX128X_DEV),
             sx128x_get_coding_rate(&SX128X_DEV),
             sx128x_get_bandwidth(&SX128X_DEV),
             sx128x_get_crc(&SX128X_DEV),
             sx128x_get_preamble_length(&SX128X_DEV),
             sx128x_get_fixed_header_len_mode(&SX128X_DEV)

    );
    LOG_INFO("LoRa driver working with interrupt: %d\n", SX128X_USE_INTERRUPT);

    return RADIO_RESULT_OK;
}

/* ------------------------------ RADIO DRIVER ------------------------------ */
const struct radio_driver sx128x_radio_driver = {
    sx128x_initialization,
    sx128x_prepare,
    sx128x_transmit,
    sx128x_send,
    sx128x_read_packet,
    sx128x_clear_channel_assesment,
    sx128x_receiving_packet,
    sx128x_pending_packet,
    sx128x_on,
    sx128x_off,
    sx128x_get_value,
    sx128x_set_value,
    sx128x_get_object,
    sx128x_set_object,
};
