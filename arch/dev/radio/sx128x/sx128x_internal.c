/*
 * Copyright (c) 2016 Unwired Devices <info@unwds.com>
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
 * @brief       implementation of internal functions for sx128x
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

#include "dev/spi.h"
#include "dev/gpio-hal.h"

#include "sx128x.h"

#include "sys/_stdint.h"
/* #include "sx128x_params.h" */

#define LOG_MODULE "SX128X-INTERNAL"
#ifndef LOG_CONF_LEVEL_SX128X_INTERNAL
#define LOG_CONF_LEVEL_SX128X_INTERNAL LOG_LEVEL_INFO
#endif
#define LOG_LEVEL LOG_CONF_LEVEL_SX128X_INTERNAL

/* Zet op 1 om vóór elke SPI-transactie de BUSY-pin-status te loggen.
 * Handig om te bevestigen of BUSY nog hoog staat (chip niet klaar)
 * op het moment dat we toch al beginnen te klokken. Zet terug op 0
 * zodra de diagnose klaar is, dit is vrij spammy in de logs. */
#ifndef SX128X_DEBUG_BUSY
#define SX128X_DEBUG_BUSY 1
#endif

static void sx128x_wait_busy(const sx128x_t *dev) {
    uint32_t timeout = 100000;

#if SX128X_DEBUG_BUSY
    if (gpio_hal_arch_read_pin(0, dev->params.busy_pin)) {
        printf("[sx128x] BUSY was HIGH at wait_busy entry\n");
    }
#endif

    while(gpio_hal_arch_read_pin(0, dev->params.busy_pin) && timeout > 0) {
        timeout--;
    }
    if(timeout == 0) {
        printf("[sx128x] ERROR: BUSY pin timeout!\n");
    }
}

void sx128x_reg_write(const sx128x_t *dev, uint16_t addr, uint8_t data)
{
    sx128x_reg_write_burst(dev, addr, &data, 1);
}

uint8_t sx128x_reg_read(const sx128x_t *dev, uint16_t addr)
{
    return sx128x_reg_read_burst(dev, addr);
}

void sx128x_cmd_burst(const sx128x_t *dev, uint8_t cmd, uint8_t *in, uint8_t in_size, uint8_t *out, uint8_t out_size)
{
    /* NIEUW: wacht tot de chip klaar is met de vorige opdracht
     * VOORDAT we CS asserteren voor een nieuwe transactie. Zonder
     * dit kan de chip nog bezig zijn en reageert de SPI-interface
     * niet (correct) op het nieuwe commando -> vaste/foutieve
     * garbage-waarden terug (bv. 0x4343). */
    sx128x_wait_busy(dev);

    spi_select(&dev->params.spi);

    spi_write_byte(&dev->params.spi, cmd);

    if (in_size) {
        spi_write(&dev->params.spi, in, in_size);
    }
    if (out_size) {
        spi_read(&dev->params.spi, out, out_size);
    }

    spi_deselect(&dev->params.spi);

    sx128x_wait_busy(dev);
}

void sx128x_reg_write_burst(const sx128x_t *dev, uint16_t reg, uint8_t *buffer,
                            uint8_t size)
{
    uint8_t reg_addr[2] = { (reg >> 8) & 0xFF, (reg & 0xFF) };

    sx128x_wait_busy(dev);   /* NIEUW */

    spi_select(&dev->params.spi);

    uint8_t cmd = SX128X_CMD_WRITE_REG;
    spi_transfer(&dev->params.spi, &cmd, sizeof(uint8_t), NULL, 0, 0);
    spi_transfer(&dev->params.spi, reg_addr, sizeof(uint16_t), NULL, 0, 0);
    spi_transfer(&dev->params.spi, buffer, size, NULL, 0, 0);

    spi_deselect(&dev->params.spi);

    sx128x_wait_busy(dev);   /* NIEUW */
}

uint8_t sx128x_reg_read_burst(const sx128x_t *dev, uint16_t reg)
{
    uint8_t ret;
    uint8_t reg_addr[2] = { (reg >> 8) & 0xFF, (reg & 0xFF) };

    sx128x_wait_busy(dev);   /* NIEUW */

    spi_select(&dev->params.spi);

    uint8_t cmd = SX128X_CMD_READ_REG;
    spi_transfer(&dev->params.spi, &cmd, sizeof(uint8_t), NULL, 0, 0);
    spi_transfer(&dev->params.spi, reg_addr, sizeof(uint16_t), NULL, 0, 0);
    spi_transfer(&dev->params.spi, NULL, 0, &ret, 1, 0);

    spi_deselect(&dev->params.spi);

    sx128x_wait_busy(dev);   /* NIEUW */

    return ret;
}

void sx128x_write_fifo(const sx128x_t *dev, uint8_t *buffer, uint8_t size)
{
    uint8_t cmd = SX128X_CMD_WRITE_BUF;
    uint8_t offset = 0;

    sx128x_wait_busy(dev);   /* NIEUW */

    spi_select(&dev->params.spi);

    spi_transfer(&dev->params.spi, &cmd, 1, NULL, 0, 0);
    spi_transfer(&dev->params.spi, &offset, 1, NULL, 0, 0);
    spi_transfer(&dev->params.spi, buffer, size, NULL, 0, 0);

    spi_deselect(&dev->params.spi);

    sx128x_wait_busy(dev);   /* NIEUW */
}

void sx128x_read_fifo(const sx128x_t *dev, uint8_t *buffer, uint8_t size)
{
    uint8_t cmd = SX128X_CMD_READ_BUF;
    uint8_t offset = 0;

    sx128x_wait_busy(dev);   /* NIEUW */

    spi_select(&dev->params.spi);

    spi_transfer(&dev->params.spi, &cmd, 1, NULL, 0, 0);
    spi_transfer(&dev->params.spi, &offset, 1, NULL, 0, 0);
    spi_transfer(&dev->params.spi, &size, 1, NULL, 0, 0);
    spi_transfer(&dev->params.spi, NULL, 0, buffer, size, 0);

    spi_deselect(&dev->params.spi);

    sx128x_wait_busy(dev);   /* NIEUW */
}
