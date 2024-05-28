/*
 * Copyright (C) 2016 Unwired Devices <info@unwds.com>
 *               2017 Inria Chile
 *               2017 Inria
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     drivers_sx128x
 * @{
 * @file
 * @brief       Implementation of get and set functions for sx128x
 *
 * @author      Eugene P. <ep@unwds.com>
 * @author      José Ignacio Alamos <jose.alamos@inria.cl>
 * @author      Alexandre Abadie <alexandre.abadie@inria.fr>
 * @}
 */

#include <math.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>

#include "sys/_stdint.h"
#include "sys/log.h"

#include "sx128x.h"

#include "sys/log.h"
#define LOG_MODULE "SX128X GETSET"
#define LOG_LEVEL LOG_CONF_LEVEL_RADIO

/*--------------------------- HELPER FUNCTIONS -------------------------------*/

uint8_t _sx128x_get_timeout_in_s(uint8_t periodBase, uint16_t periodBaseCount)
{
    LOG_FUNC("Function call: %s\n", __func__);
    switch (periodBase)
    {
    case 0:
        return 0.000015625 * periodBaseCount;
    case 1:
        return 0.0000625 * periodBaseCount;
    case 2:
        return 0.001 * periodBaseCount;
    case 3:
        return 0.004 * periodBaseCount;
    default:
        return 0;
    }
}

uint16_t _sx128x_get_ramp_time_in_micro_s(sx128x_tx_ramp_times_t ramp_time)
{
    LOG_FUNC("Function call: %s\n", __func__);
    switch (ramp_time)
    {
    case TX_RADIO_RAMP_02_US:
        return 2;
    case TX_RADIO_RAMP_04_US:
        return 4;
    case TX_RADIO_RAMP_06_US:
        return 6;
    case TX_RADIO_RAMP_08_US:
        return 8;
    case TX_RADIO_RAMP_10_US:
        return 10;
    case TX_RADIO_RAMP_12_US:
        return 12;
    case TX_RADIO_RAMP_16_US:
        return 16;
    case TX_RADIO_RAMP_20_US:
        return 20;
    default:
        return 34464;
    }
}

/*------------------------------ HARDWARE ------------------------------------*/

void sx128x_cmd_set_buffer_base_address(const sx128x_t *dev, uint8_t tx_address, uint8_t rx_address)
{
    LOG_FUNC("Function call: %s\n", __func__);
    uint8_t args[2] = {tx_address, rx_address};

    sx128x_cmd_burst(dev, SX128X_CMD_SET_BUFFER_BASE_ADDRESS, args, 2, NULL, 0);
}

uint8_t sx128x_cmd_get_rx_buffer_status(sx128x_t *dev)
{
    LOG_FUNC("Function call: %s\n", __func__);
    (void)dev;
    uint8_t nop = 0;
    uint8_t status[2];
    sx128x_cmd_burst(dev, SX128X_CMD_GET_RX_BUFFER_STATUS, &nop, 1, status, 2);

    switch (dev->settings.packet_type)
    {
    case SX128X_PACKET_TYPE_LORA:
        dev->_internal.rx_length = status[0];
        break;
    default:
        LOG_DBG("Not supported packet type\n");
        break;
    }

    return dev->_internal.rx_length;
}

void sx128x_cmd_set_dio_irq_params(const sx128x_t *dev, uint16_t dio1_mask, uint16_t dio2_mask, uint16_t dio3_mask)
{
    LOG_FUNC("Function call: %s\n", __func__);

    // before setting new mask: clear all IRQ's
    sx128x_cmd_clear_irq_status(dev, SX128X_IRQ_REG_ALL);

    switch (dev->settings.packet_type)
    {
    case SX128X_PACKET_TYPE_LORA:;
        uint16_t mask = (dio1_mask | dio2_mask | dio3_mask) & SX128X_IRQ_REG_LORA_MASK;
        LOG_DBG("Set irq params: mask=%d, dio1=%d, dio2=%d, dio3=%d\n", mask, dio1_mask, dio2_mask, dio3_mask);
        uint8_t params[8] = {
            (mask & 0xFF00) >> 8,
            (mask & 0xFF),
            (dio1_mask & 0xFF00) >> 8,
            (dio1_mask & 0xFF),
            (dio2_mask & 0xFF00) >> 8,
            (dio2_mask & 0xFF),
            (dio3_mask & 0xFF00) >> 8,
            (dio3_mask & 0xFF),
        };
        sx128x_cmd_burst(dev, SX128X_CMD_SET_DIO_IRQ_PARAMS, (uint8_t *)params, 8, NULL, 0);

        break;
    default:
        LOG_DBG("Not supported packet type\n");
        break;
    }
}

uint16_t sx128x_cmd_get_irq_status(const sx128x_t *dev)
{
    LOG_FUNC("Function call: %s\n", __func__);
    uint8_t nop = 0;
    uint8_t ret[2];

    sx128x_cmd_burst(dev, SX128X_CMD_GET_IRQ_STATUS, &nop, 1, ret, 2);

    return (ret[0] << 8) | ret[1];
}

void sx128x_cmd_clear_irq_status(const sx128x_t *dev, uint16_t irq_mask)
{
    LOG_FUNC("Function call: %s\n", __func__);
    uint8_t out[2] = {(irq_mask & 0xFF00) >> 8, irq_mask & 0xFF};
    sx128x_cmd_burst(dev, SX128X_CMD_CLR_IRQ_STATUS, out, 2, NULL, 0);
}

void sx128x_cmd_get_packet_status(sx128x_t *dev)
{
    LOG_FUNC("Function call: %s\n", __func__);
    uint8_t nop = 0;
    uint8_t status[5];
    sx128x_cmd_burst(dev, SX128X_CMD_GET_PACKET_STATUS, &nop, 1, status, 5);

    switch (dev->settings.packet_type)
    {
    case SX128X_PACKET_TYPE_LORA:
        dev->_internal.rx_rssi = -status[0] / 2;
        dev->_internal.rx_snr = status[1] / 4;
        break;
    default:
        LOG_DBG("Not supported packet type\n");
        break;
    }
}

void sx128x_cmd_write_buffer(const sx128x_t *dev, uint8_t *buf, size_t len)
{
    LOG_FUNC("Function call: %s\n", __func__);
    sx128x_cmd_burst(dev, SX128X_CMD_WRITE_BUF, buf, len, NULL, 0);
}

uint8_t sx128x_cmd_get_status(const sx128x_t *dev)
{
    LOG_FUNC("Function call: %s\n", __func__);
    uint8_t ret;
    sx128x_cmd_burst(dev, SX128X_CMD_GET_STATUS, NULL, 0, &ret, 1);
    return ret;
}

/*-------------------------------- OPMODES -----------------------------------*/
/*-------------- TX --------------*/

void sx128x_cmd_set_tx(const sx128x_t *dev, uint8_t period_base, uint16_t period_base_count)
{
    LOG_FUNC("Function call: %s\n", __func__);

    sx128x_set_state_event(&SX128X_DEV, SX128X_NO_EVENT);

    // prepare params to set device in TX mode
    uint8_t params[3] = {period_base, (period_base_count >> 8) & 0xFF, (period_base_count & 0xFF)};

    // set device in TX mode
    sx128x_cmd_burst(dev, SX128X_CMD_SET_TX, params, 3, NULL, 0);

    // store internal timestamp of TX start
    SX128X_DEV._internal.tx_timestamp = RTIMER_NOW();
}

/*-------------- RX --------------*/

void sx128x_configure_rx(sx128x_t *dev)
{
    LOG_FUNC("Function call: %s\n", __func__);
    LOG_DBG("configuring device for RX mode\n");
    sx128x_set_state_event(&SX128X_DEV, SX128X_NO_EVENT);
    sx128x_cmd_set_buffer_base_address(dev, 0, 0);
    switch (dev->settings.packet_type)
    {
    case SX128X_PACKET_TYPE_LORA:
        break;
    default:
        LOG_DBG("Unsupported packet type\n");
        break;
    }
}

void _sx128x_cmd_set_rx(sx128x_t *dev, uint8_t period_base, uint16_t period_base_count)
{
    LOG_FUNC("Function call: %s\n", __func__);
    // set current RX state to listening
    sx128x_set_state_rx(dev, SX128X_RX_LISTENING);

    // prepare params to set device in RX mode
    uint8_t params[3] = {period_base, (period_base_count >> 8) & 0xFF, (period_base_count & 0xFF)};

    // set device in RX mode
    sx128x_cmd_burst(dev, SX128X_CMD_SET_RX, params, 3, NULL, 0);

    // store internal timestamp of RX start
    SX128X_DEV._internal.rx_timestamp = RTIMER_NOW();
}

/*-------------- CAD --------------*/

void sx128x_set_cad(sx128x_t *dev)
{
    LOG_FUNC("Function call: %s\n", __func__);
    sx128x_set_state_event(&SX128X_DEV, SX128X_NO_EVENT);
    switch (dev->settings.packet_type)
    {
    case SX128X_PACKET_TYPE_LORA:
    {
        break;
    }
    default:
        LOG_DBG("Unsupported packet type\n");
        return;
    }

    sx128x_set_state_opmode(dev, SX128X_OPMODE_CAD);
}

void sx128x_cmd_set_cad_params(sx128x_t *dev, uint8_t symbol_num)
{
    LOG_FUNC("Function call: %s\n", __func__);
    dev->settings.lora.cad_symbols = symbol_num;
    sx128x_cmd_burst(dev, SX128X_CMD_SET_CAD_PARAMS, &symbol_num, 1, NULL, 0);
}

void sx128x_cmd_set_cad(const sx128x_t *dev)
{
    LOG_FUNC("Function call: %s\n", __func__);

    sx128x_cmd_burst(dev, SX128X_CMD_SET_CAD, NULL, 0, NULL, 0);
    SX128X_DEV._internal.cad_timestamp = RTIMER_NOW();
}

/*-------------- STANDBY --------------*/

void _cmd_sx128x_set_standby(const sx128x_t *dev, uint8_t config)
{
    LOG_FUNC("Function call: %s\n", __func__);
    LOG_DBG("Cmd set standby with config %#02x\n", config);
    sx128x_cmd_burst(dev, SX128X_CMD_SET_STANDBY, &config, 1, NULL, 0);
}

void sx128x_set_standby(sx128x_t *dev)
{
    LOG_FUNC("Function call: %s\n", __func__);
    (void)(dev);
    LOG_DBG("Set standby\n");

    // sx128x_set_state_rx(&SX128X_DEV, SX128X_RX_OFF);
    sx128x_set_state_opmode(dev, SX128X_OPMODE_STANDBY);
}

/*-------------- SLEEP --------------*/

void sx128x_cmd_set_sleep(const sx128x_t *dev, uint8_t config)
{
    LOG_FUNC("Function call: %s\n", __func__);
    LOG_DBG("Cmd set sleep with config %#02x\n", config);
    // TODO Check we are on STDBY mode
    (void)(dev);
    // sx128x_cmd_burst(dev, SX128X_CMD_SET_SLEEP, &config, 1, NULL, 0);
}

void sx128x_set_sleep(sx128x_t *dev)
{
    LOG_FUNC("Function call: %s\n", __func__);
    (void)(dev);

    sx128x_set_state_opmode(dev, SX128X_OPMODE_SLEEP);
}

/*----------------------------  DEVICE STATE ---------------------------------*/

void sx128x_set_state_rx(sx128x_t *dev, sx128x_rx_state_t rx)
{
    LOG_FUNC("Function call: %s\n", __func__);
    switch (rx)
    {
    case SX128X_RX_OFF:
        LOG_DBG("set RX state to: off\n");
        dev->state.rx = SX128X_RX_OFF;
        break;
    case SX128X_RX_LISTENING:
        LOG_DBG("set RX state to: listening\n");
        dev->state.rx = SX128X_RX_LISTENING;
        break;
    case SX128X_RX_RECEIVING:
        if (dev->state.rx != SX128X_RX_LISTENING)
        {
            LOG_ERR("[rx_state] Went from '%d' directly to to 'receiving'\n", dev->state.rx);
        }
        dev->state.rx = SX128X_RX_RECEIVING;
        LOG_DBG("set RX state to: receiving\n");
        break;
    case SX128X_RX_RECEIVED:
#if SX128X_BUSY_RX
        if (dev->state.rx != sx128x_rx_receiving)
        {
            LOG_WARN("[rx_state] Went to 'received' without 'receiving'\n");
        }
#endif
        dev->state.rx = SX128X_RX_RECEIVED;
        LOG_DBG("set RX state to: received\n");
        break;
    case SX128X_RX_READ:
        if (dev->state.rx != SX128X_RX_RECEIVED)
        {
            LOG_WARN("[rx_state] read the content of the module without packet pending\n");
        }
        dev->state.rx = SX128X_RX_READ;
        LOG_DBG("set RX state to: read\n");

        break;
    default:
        dev->state.rx = rx;
    }
}
uint8_t sx128x_get_state_rx(const sx128x_t *dev)
{
    LOG_FUNC("Function call: %s\n", __func__);
    return dev->state.rx;
}

void sx128x_set_state_opmode(sx128x_t *dev, sx128x_opmode_t op_mode)
{
    LOG_FUNC("Function call: %s\n", __func__);
    sx128x_set_state_event(&SX128X_DEV, SX128X_NO_EVENT);

    dev->state.opmode = op_mode;
    switch (op_mode)
    {
    case SX128X_OPMODE_SLEEP:
        LOG_DBG("Set op mode: SLEEP\n");
        sx128x_cmd_set_sleep(dev, 0);
        break;
    case SX128X_OPMODE_STANDBY:
        LOG_DBG("set op mode: STANDBY\n");
        _cmd_sx128x_set_standby(dev, 0);
        break;
    case SX128X_OPMODE_CAD:
        LOG_DBG("set op mode: CAD\n");
        sx128x_cmd_set_cad(dev);
        break;
    case SX128X_OPMODE_TX:
        LOG_DBG("set op mode: TRANSMITTER WITH TIMOUT %d\n", _sx128x_get_timeout_in_s(PERIOD_BASE_04_MS, 2500));
        sx128x_cmd_set_tx(dev, PERIOD_BASE_04_MS, 2500);
        break;
    case SX128X_OPMODE_RX_SINGLE:
        LOG_DBG("set op mode: RECEIVER SINGLE\n");
        sx128x_configure_rx(dev);
        _sx128x_cmd_set_rx(dev, 0, 0);
        break;
    case SX128X_OPMODE_RX:
        LOG_DBG("set op mode: RECEIVER WITH TIMEOUT %d\n", _sx128x_get_timeout_in_s(PERIOD_BASE_04_MS, 2500));
        sx128x_configure_rx(dev);
        _sx128x_cmd_set_rx(dev, PERIOD_BASE_04_MS, 2500);
        break;
    case SX128X_OPMODE_RX_ACK:
        LOG_DBG("set op mode: RECEIVER WITH TIMEOUT %d\n", _sx128x_get_timeout_in_s(PERIOD_BASE_04_MS, 2500));
        sx128x_configure_rx(dev);
        _sx128x_cmd_set_rx(dev, PERIOD_BASE_04_MS, 2500);
        break;
    case SX128X_OPMODE_RX_CONTINUOUS:
        LOG_DBG("set op mode: RECEIVER CONTINUOUS\n");
        sx128x_configure_rx(dev);
        _sx128x_cmd_set_rx(dev, 0, 65535);
        break;
    default:
        LOG_DBG("\n\n\nset op mode: UNKNOWN (%d)\n", op_mode);
        break;
    }
}
uint8_t sx128x_get_state_opmode(const sx128x_t *dev)
{
    LOG_FUNC("Function call: %s\n", __func__);
    return dev->state.opmode;
}

void sx128x_set_state_event(sx128x_t *dev, sx128x_event_state_t event)
{
    LOG_FUNC("Function call: %s\n", __func__);
    dev->state.event = event;
}
uint8_t sx128x_get_state_event(const sx128x_t *dev)
{
    LOG_FUNC("Function call: %s\n", __func__);
    return dev->state.event;
}

/*------------------------------- DEVICE SETTINGS ----------------------------*/

void sx128x_set_modem(sx128x_t *dev, sx128x_packet_type_t packet_type)
{
    LOG_FUNC("Function call: %s\n", __func__);
    LOG_DBG("set packet_type: %d\n", packet_type);
    dev->settings.packet_type = packet_type;
}
void sx128x_cmd_set_packet_type(sx128x_t *dev, sx128x_packet_type_t packet_type)
{
    LOG_FUNC("Function call: %s\n", __func__);
    LOG_DBG("Set packet type: %d\n", packet_type);
    if (packet_type != SX128X_PACKET_TYPE_LORA)
    {
        LOG_DBG("LoRa packet type supported only");
    }
    sx128x_cmd_burst(dev, SX128X_CMD_SET_PACKET_TYPE, &packet_type, 1, NULL, 0);
    dev->settings.packet_type = packet_type;
}
uint8_t sx128x_cmd_get_packet_type(const sx128x_t *dev)
{
    LOG_FUNC("Function call: %s\n", __func__);
    uint8_t ret;
    uint8_t nop = 0;
    sx128x_cmd_burst(dev, SX128X_CMD_GET_PACKET_TYPE, &nop, 1, &ret, 1);
    return ret;
}

void sx128x_cmd_set_regulator_mode(sx128x_t *dev, sx128x_regulator_mode_t mode)
{
    LOG_FUNC("Function call: %s\n", __func__);
    dev->settings.regulator_mode = mode;
    sx128x_cmd_burst(dev, SX128X_CMD_SET_REGULATOR_MODE, &mode, 1, NULL, 0);
}

void sx128x_cmd_set_tx_params(sx128x_t *dev, int8_t power, sx128x_tx_ramp_times_t ramp_time)
{
    LOG_FUNC("Function call: %s\n", __func__);
    if (power < -18 || power > 13)
    {
        LOG_DBG("Out of range power range");
        return;
    }
    LOG_DBG("TX params: power=%d dBm, ramp_time=%d µs\n", power, _sx128x_get_ramp_time_in_micro_s(ramp_time));
    uint8_t params[2] = {(uint8_t)(power + 18), ramp_time}; // + 18 because the device does -18 => -18 dBm to + 12.5 dBm = 0 - 31

    dev->settings.power = power;
    dev->settings.ramp_time = ramp_time;

    sx128x_cmd_burst(dev, SX128X_CMD_SET_TX_PARAMS, params, 2, NULL, 0);
}
uint8_t sx128x_get_tx_power(const sx128x_t *dev)
{
    LOG_FUNC("Function call: %s\n", __func__);
    (void)(dev);
    return dev->settings.power;
}
sx128x_tx_ramp_times_t sx128x_get_tx_ramp_time(const sx128x_t *dev)
{
    LOG_FUNC("Function call: %s\n", __func__);
    (void)(dev);
    return dev->settings.ramp_time;
}

void sx128x_cmd_set_frequency(sx128x_t *dev, uint32_t freq)
{
    LOG_FUNC("Function call: %s\n", __func__);
    // Freq in range (2400..2500)
    dev->settings.lora.frequency = freq;
    uint32_t step = (freq * (1 << SX128X_DIV_EXPONANT)) / SX128X_CRYSTAL_FREQ;
    uint8_t data[3] = {(uint8_t)((step >> 16) & 0xFF), (uint8_t)((step >> 8) & 0xFF), (uint8_t)(step & 0xFF)};
    sx128x_cmd_burst(dev, SX128X_CMD_SET_RF_FREQUENCY, data, 3, NULL, 0);
}
uint32_t sx128x_get_frequency(const sx128x_t *dev)
{
    LOG_FUNC("Function call: %s\n", __func__);
    return dev->settings.lora.frequency;
}

void sx128x_set_rx_mode(sx128x_t *dev, sx128x_rx_mode_t rx_mode)
{
    LOG_FUNC("Function call: %s\n", __func__);
    (void)(dev);
    LOG_DBG("Set RX mode: %d\n", rx_mode);
    dev->settings.rx_mode = rx_mode;
}
sx128x_rx_mode_t sx128x_get_rx_mode(const sx128x_t *dev)
{
    LOG_FUNC("Function call: %s\n", __func__);
    return dev->settings.rx_mode;
}

/*---------------------------- DEVICE SETTINGS: LORA -------------------------*/
void sx128x_set_preamble_length(sx128x_t *dev, uint16_t preamble)
{
    LOG_FUNC("Function call: %s\n", __func__);
    (void)(dev);
    LOG_DBG("Set preamble length: %d\n", preamble);

    dev->settings.lora.preamble_len = preamble;
    sx128x_cmd_set_packet_params(dev, preamble,
                                 sx128x_get_fixed_header_len_mode(dev), 0,
                                 sx128x_get_crc(dev), sx128x_get_iq_inverted(dev), 0, 0);
}
uint16_t sx128x_get_preamble_length(const sx128x_t *dev)
{
    LOG_FUNC("Function call: %s\n", __func__);
    (void)(dev);
    return dev->settings.lora.preamble_len;
}

void sx128x_set_bandwidth(sx128x_t *dev, LoRa_bandwidths bandwidth)
{
    LOG_FUNC("Function call: %s\n", __func__);
    (void)(dev);
    LOG_DBG("Set bandwidth: %d\n", bandwidth);
    sx128x_cmd_set_modulation_params(dev, sx128x_get_spreading_factor(dev), bandwidth, sx128x_get_coding_rate(dev));
}
LoRa_bandwidths sx128x_get_bandwidth(const sx128x_t *dev)
{
    LOG_FUNC("Function call: %s\n", __func__);
    (void)(dev);
    return dev->settings.lora.bandwidth;
}

void sx128x_set_spreading_factor(sx128x_t *dev, LoRa_spreading_factors spreading_factor)
{
    LOG_FUNC("Function call: %s\n", __func__);
    LOG_DBG("Set spreading factor: %d\n", spreading_factor);
    sx128x_cmd_set_modulation_params(dev, spreading_factor, sx128x_get_bandwidth(dev), sx128x_get_coding_rate(dev));
}
LoRa_spreading_factors sx128x_get_spreading_factor(const sx128x_t *dev)
{
    LOG_FUNC("Function call: %s\n", __func__);
    return dev->settings.lora.spreading_factor;
}

void sx128x_set_coding_rate(sx128x_t *dev, LoRa_coding_rates coderate)
{
    LOG_FUNC("Function call: %s\n", __func__);
    (void)(dev);
    LOG_DBG("Set coding rate: %d\n", coderate);
    sx128x_cmd_set_modulation_params(dev, sx128x_get_spreading_factor(dev), sx128x_get_bandwidth(dev), coderate);
}
LoRa_coding_rates sx128x_get_coding_rate(const sx128x_t *dev)
{
    LOG_FUNC("Function call: %s\n", __func__);
    (void)(dev);
    return dev->settings.lora.coderate;
}

void sx128x_cmd_set_modulation_params(sx128x_t *dev, LoRa_spreading_factors SF, LoRa_bandwidths BW, LoRa_coding_rates code_rate)
{
    LOG_FUNC("Function call: %s\n", __func__);
    uint8_t params[3];
    uint8_t sf_reg = 0;

    switch (dev->settings.packet_type)
    {
    case SX128X_PACKET_TYPE_LORA:

        // set spreading factor
        dev->settings.lora.spreading_factor = SF;
        params[0] = SF;
        // set bandwidth
        dev->settings.lora.bandwidth = BW;
        params[1] = BW;
        // set code rate
        dev->settings.lora.coderate = code_rate;
        params[2] = code_rate;

        // log parameters
        LOG_INFO("%d,%d,%d,\n\n", params[0], params[1], params[2]);

        // set starting address WriteRegister based on the SF
        switch (SF)
        {
        case LORA_SF_5:
            sf_reg = 0x1E;
            break;
        case LORA_SF_6:
            sf_reg = 0x1E;
            break;
        case LORA_SF_7:
            sf_reg = 0x37;
            break;
        case LORA_SF_8:
            sf_reg = 0x37;
            break;
        case LORA_SF_9:
            sf_reg = 0x32;
            break;
        case LORA_SF_10:
            sf_reg = 0x32;
            break;
        case LORA_SF_11:
            sf_reg = 0x32;
            break;
        case LORA_SF_12:
            sf_reg = 0x32;
            break;
        default:
            sf_reg = 0x37;
            break;
        }

        // send parameters to the device
        sx128x_cmd_burst(dev, SX128X_CMD_SET_MODULATION_PARAMS, params, 3, NULL, 0);
        if (sf_reg)
        {
            sx128x_reg_write_burst(dev, 0x925, &sf_reg, 1);
        }
        return;
    default:
        LOG_DBG("Unsupported packet type\n");
        return;
    }
}

static inline void _set_flag(sx128x_t *dev, uint8_t flag, bool value)
{
    LOG_FUNC("Function call: %s\n", __func__);
    (void)(dev);
    if (value)
    {
        dev->settings.lora.flags |= flag;
    }
    else
    {
        dev->settings.lora.flags &= ~flag;
    }
}

void sx128x_cmd_set_packet_params(sx128x_t *dev, uint8_t preamble_len, uint8_t enable_fixed_header_len, uint8_t payload_len, uint8_t enable_crc, uint8_t iq_inverted, uint8_t param6, uint8_t param7)
{
    LOG_FUNC("Function call: %s\n", __func__);
    (void)param6;
    (void)param7;
    uint8_t params[7];

    LOG_DBG("Packet params: ");

    switch (dev->settings.packet_type)
    {
    case SX128X_PACKET_TYPE_LORA:
        dev->settings.lora.preamble_len = preamble_len;
        // TODO bit 7:4 represent the exponant in [0:3] * 2^[7:4]
        params[0] = preamble_len;
        LOG_DBG_("preamble length=%d, ", params[0]);
        _set_flag(dev, SX128X_FLAG_ENABLE_FIXED_HEADER_LENGTH, (bool)enable_fixed_header_len);
        if (enable_fixed_header_len)
        {
            params[1] = SX128X_LORA_IMPLICIT_HEADER;
            LOG_DBG_("Implicit Header, ");
        }
        else
        {
            params[1] = SX128X_LORA_EXPLICIT_HEADER;
            LOG_DBG_("Explicit Header, ");
        }
        params[2] = payload_len; // payload length
        LOG_DBG_("Payload length=%d, ", params[2]);
        dev->settings.lora.payload_length = payload_len;
        _set_flag(dev, SX128X_FLAG_ENABLE_CRC, (bool)enable_crc);
        if (enable_crc)
        {
            params[3] = SX128X_LORA_CRC_ENABLE;
            LOG_DBG_("CRC enabled, ");
        }
        else
        {
            params[3] = SX128X_LORA_CRC_DISABLE;
            LOG_DBG_("CRC disabled, ");
        }
        _set_flag(dev, SX128X_FLAG_IQ_INVERTED, (bool)iq_inverted);
        if (iq_inverted)
        {
            params[4] = SX128X_LORA_IQ_INVERTED;
            LOG_DBG_("IQ Inverted\n");
        }
        else
        {
            params[4] = SX128X_LORA_IQ_STD;
            LOG_DBG_("IQ STD\n");
        }
        params[5] = 0;
        params[6] = 0;
        return sx128x_cmd_burst(dev, SX128X_CMD_SET_PACKET_PARAMS, params, 7, NULL, 0);

    default:
        LOG_DBG("Not supported packet type\n");
        return;
    }
}

void sx128x_set_payload_length(sx128x_t *dev, uint8_t payload_len)
{
    LOG_FUNC("Function call: %s\n", __func__);
    (void)(dev);
    LOG_DBG("Set payload length: %d\n", payload_len);
    dev->settings.lora.payload_length = payload_len;
    sx128x_cmd_set_packet_params(dev, sx128x_get_preamble_length(dev),
                                 sx128x_get_fixed_header_len_mode(dev), payload_len,
                                 sx128x_get_crc(dev), sx128x_get_iq_inverted(dev), 0, 0);
}
uint8_t sx128x_get_payload_length(const sx128x_t *dev)
{
    LOG_FUNC("Function call: %s\n", __func__);
    return (dev->settings.lora.payload_length);
}

void sx128x_set_crc(sx128x_t *dev, bool crc)
{
    LOG_FUNC("Function call: %s\n", __func__);
    (void)(dev);
    LOG_DBG("Set CRC: %d\n", crc);
    _set_flag(dev, SX128X_FLAG_ENABLE_CRC, crc);
    sx128x_cmd_set_packet_params(dev, sx128x_get_preamble_length(dev),
                                 sx128x_get_fixed_header_len_mode(dev), 0,
                                 crc, sx128x_get_iq_inverted(dev), 0, 0);
}
bool sx128x_get_crc(const sx128x_t *dev)
{
    LOG_FUNC("Function call: %s\n", __func__);
    return (dev->settings.lora.flags & SX128X_FLAG_ENABLE_CRC);
}

void sx128x_set_fixed_header_len_mode(sx128x_t *dev, bool fixed_len)
{
    LOG_FUNC("Function call: %s\n", __func__);
    (void)(dev);
    LOG_DBG("Set fixed header length: %d\n", fixed_len);

    _set_flag(dev, SX128X_FLAG_ENABLE_FIXED_HEADER_LENGTH, fixed_len);
    sx128x_cmd_set_packet_params(dev, sx128x_get_preamble_length(dev), fixed_len,
                                 0, sx128x_get_crc(dev), sx128x_get_iq_inverted(dev), 0, 0);
}
bool sx128x_get_fixed_header_len_mode(const sx128x_t *dev)
{
    LOG_FUNC("Function call: %s\n", __func__);
    return dev->settings.lora.flags & SX128X_FLAG_ENABLE_FIXED_HEADER_LENGTH;
}

void sx128x_set_iq_inverted(sx128x_t *dev, bool iq_invert)
{
    LOG_FUNC("Function call: %s\n", __func__);
    (void)(dev);
    LOG_DBG("Set IQ invert: %d\n", iq_invert);

    _set_flag(dev, SX128X_FLAG_IQ_INVERTED, iq_invert);
    sx128x_cmd_set_packet_params(dev, sx128x_get_preamble_length(dev),
                                 sx128x_get_fixed_header_len_mode(dev), 0,
                                 sx128x_get_crc(dev), iq_invert, 0, 0);
}
bool sx128x_get_iq_inverted(const sx128x_t *dev)
{
    LOG_FUNC("Function call: %s\n", __func__);
    return dev->settings.lora.flags & SX128X_FLAG_IQ_INVERTED;
}
