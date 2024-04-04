
#ifndef SX128X_H
#define SX128X_H

#include <stdint.h>
#include "contiki.h"
#include "dev/radio.h"
#include "dev/spi.h"
#include "dev/gpio-hal.h"
#include "sys/_stdint.h"

#include "sx128x_pinout.h"
#include "sx128x_registers.h"
#include "lora24.h"

#ifdef __cplusplus
extern "C" {
#endif

//  spi
# if 1
#ifdef SX128X_SPI_BITRATE_CONF
#define SX128X_SPI_BITRATE SX128X_SPI_BITRATE_CONF
#else
//#define SX128X_SPI_BITRATE 40000000
#define SX128X_SPI_BITRATE 8000000
#endif

#ifdef SX128X_SPI_PHASE_CONF
#define SX128X_SPI_PHASE SX128X_SPI_PHASE_CONF
#else
#define SX128X_SPI_PHASE 0
#endif

#ifdef SX128X_SPI_POL_CONF
#define SX128X_SPI_POL SX128X_SPI_POL_CONF
#else
#define SX128X_SPI_POL 0
#endif

#ifdef SX128X_SPI_CONTROLLER_CONF
#define SX128X_SPI_CONTROLLER SX128X_SPI_CONTROLLER_CONF
#else
#define SX128X_SPI_CONTROLLER 1
#endif
#endif

// extra functionalities like busy, interrupts, header detection
#if 1
#ifdef SX128X_BUSY_RX_CONF
#define SX128X_BUSY_RX SX128X_BUSY_RX_CONF
#else
#ifndef MAC_CONF_WITH_CSMA
#define SX128X_BUSY_RX 0//was 1 for testing irq, 0 to enable interrupt (receiving with interrupt not yet written)
#else
#define SX128X_BUSY_RX 0
#endif
#endif

#if SX128X_USE_INTERRUPT_CONF
#define SX128X_USE_INTERRUPT SX128X_USE_INTERRUPT_CONF
#else
#define SX128X_USE_INTERRUPT 1
#endif

#if SX128X_USE_INTERRUPT
#define SX128X_DIO1_PORT_BASE GPIO_PORT_TO_BASE(SX128X_DIO1_PORT)
#define SX128X_DIO1_PIN_MASK GPIO_PIN_MASK(SX128X_DIO1_PIN)
#endif

#if SX128X_HEADER_DETECTION_CONF
#define SX128X_HEADER_DETECTION SX128X_HEADER_DETECTION_CONF
#else
#define SX128X_HEADER_DETECTION 0
#endif

/**
 * @brief   SX128X initialization result.
 */
enum {
    SX128X_INIT_OK = 0,                /**< Initialization was successful */
    SX128X_ERR_SPI,                    /**< Failed to initialize SPI bus or CS line */
    SX128X_ERR_GPIOS,                  /**< Failed to initialize GPIOs */
    SX128X_ERR_NODEV                   /**< No valid device version found */
};


#endif

// TSCH constants
#if 1
#define SX128X_TSCH_DEFAULT_TS_MAX_TX 363776

#define SX128X_TSCH_DEFAULT_TS_CCA_OFFSET 0
#define SX128X_TSCH_DEFAULT_TS_CCA 0
#define SX128X_TSCH_DEFAULT_TS_TX_OFFSET 3500
#define SX128X_TSCH_DEFAULT_TS_RX_OFFSET (SX128X_TSCH_DEFAULT_TS_TX_OFFSET - (TSCH_CONF_RX_WAIT / 2))
#define SX128X_TSCH_DEFAULT_TS_TX_ACK_DELAY 5500
#define SX128X_TSCH_DEFAULT_TS_RX_ACK_DELAY (SX128X_TSCH_DEFAULT_TS_TX_ACK_DELAY - (TSCH_CONF_RX_WAIT / 2))
#define SX128X_TSCH_DEFAULT_TS_RX_WAIT 5000
#define SX128X_TSCH_DEFAULT_TS_ACK_WAIT 5000 // TSCH_CONF_RX_WAIT
#define SX128X_TSCH_DEFAULT_TS_RX_TX 0
#define SX128X_TSCH_DEFAULT_TS_MAX_ACK 82000
#define SX128X_TSCH_DEFAULT_TS_TIMESLOT_LENGTH                                 \
  (SX128X_TSCH_DEFAULT_TS_TX_OFFSET + SX128X_TSCH_DEFAULT_TS_MAX_TX +          \
   SX128X_TSCH_DEFAULT_TS_TX_ACK_DELAY + SX128X_TSCH_DEFAULT_TS_MAX_ACK)
#endif


#if 1
#define SX128X_PACKET_TYPE_DEFAULT       (SX128X_PACKET_TYPE_LORA) /**< Use LoRa as default packet type */
#define SX128X_CHANNEL_DEFAULT           (2400UL)                  /**< Default channel frequency, 868.3MHz (Europe) */
#define SX128X_XTAL_FREQ                 (32000000UL)              /**< Internal oscillator frequency, 32MHz */
#define SX128X_RADIO_WAKEUP_TIME         (1U)                      /**< In milliseconds [ms] */

#define SX128X_CRYSTAL_FREQ (52.0)
#define SX128X_DIV_EXPONANT (18)

#define SX128X_TX_TIMEOUT_DEFAULT        (30 * MS_PER_SEC)      /**< TX timeout, 30s */
#define SX128X_RX_SINGLE                 (false)                /**< Single byte receive mode => continuous by default */
#define SX128X_BUFFER_SIZE            (256)                  /**< RX buffer size */
#define SX128X_RADIO_TX_POWER            (13U)                  /**< Radio power in dBm */
#define SX128X_RADIO_TX_RAMP_TIME        (TX_RADIO_RAMP_02_US) /**< Power amplifier ramp time */


#define SX128X_EVENT_HANDLER_STACK_SIZE  (2048U) /**< Stack size event handler */
#endif

// sx128x state
#if 1
typedef enum {
  SX128X_OPMODE_SLEEP               = 0x00,
  SX128X_OPMODE_STANDBY,
  SX128X_OPMODE_SYNTHESIZER_TX,
  SX128X_OPMODE_TX,
  SX128X_OPMODE_SYNTHESIZER_RX,
  SX128X_OPMODE_RX,
  SX128X_OPMODE_RX_SINGLE,
  SX128X_OPMODE_RX_CONTINUOUS,
  SX128X_OPMODE_RX_ACK,
  SX128X_OPMODE_CAD,
} sx128x_opmode_t;

typedef enum {
  sx128x_rx_off,
  sx128x_rx_listening,
  sx128x_rx_receiving,
  sx128x_rx_received,
  sx128x_rx_read,
} sx128x_rx_state_t;

/**
 * @enum sx128x_event_state_t
 * @brief possible event states of the device
 */
typedef enum sx128x_event_state_t{
    SX128X_NO_EVENT = 0,               /**< No event */
    SX128X_RX_DONE,                    /**< Receiving complete */
    SX128X_TX_DONE,                    /**< Sending complete*/
    SX128X_RX_TIMEOUT,                 /**< Receiving timeout */
    SX128X_TX_TIMEOUT,                 /**< Sending timeout */
    SX128X_RX_ERROR_CRC,               /**< Receiving CRC error */
    SX128X_CAD_DONE,                   /**< Channel activity detection complete */
    SX128X_CAD_DETECTED,               /**< Channel activity detected */
}sx128x_event_state_t;

/**
 * @brief   Radio state.
 */
typedef struct {
    sx128x_opmode_t opmode;
    sx128x_rx_state_t rx;
    sx128x_event_state_t event;
} sx128x_state_t;

#endif 

// sx128x settings
#if 1
/**
 * @brief   Radio driver supported modems.
 */
typedef enum {
    SX128X_PACKET_TYPE_GFSK = 0,       /**< FSK modem driver */
    SX128X_PACKET_TYPE_LORA,           /**< LoRa modem driver */
    SX128X_PACKET_TYPE_RANGING,        /**< Ranging modem driver */
    SX128X_PACKET_TYPE_FLRC,           /**< FLRC modem driver */
    SX128X_PACKET_TYPE_BLE,            /**< BLE modem driver */
}sx128x_packet_type_t;

/**
 * @brief   radio driver regulator mode.
 */
typedef enum {
  SX128X_REGULATOR_MODE_LDO = 0x00,       /*LDO regulator mode */
  SX128X_REGULATOR_MODE_DCDC = 0x01       /*DCDC regulator mode */
}sx128x_regulator_mode_t;

/**
 * @brief   radio driver regulator mode.
 */
typedef enum {
  SX128X_RX_MODE_PERIOD = 0x00,           /*RX mode period */
  SX128X_RX_MODE_SINGLE = 0x01,           /*RX mode single */
  SX128X_RX_MODE_CONTINUOUS = 0x02        /*RX mode continuous */
}sx128x_rx_mode_t;

/**
 * @brief ramp times.
*/
typedef enum {
  /* TX params settings */
TX_RADIO_RAMP_02_US = SX128X_TX_RADIO_RAMP_02_US,
TX_RADIO_RAMP_04_US = SX128X_TX_RADIO_RAMP_04_US,
TX_RADIO_RAMP_06_US = SX128X_TX_RADIO_RAMP_06_US,
TX_RADIO_RAMP_08_US = SX128X_TX_RADIO_RAMP_08_US,
TX_RADIO_RAMP_10_US = SX128X_TX_RADIO_RAMP_10_US,
TX_RADIO_RAMP_12_US = SX128X_TX_RADIO_RAMP_12_US,
TX_RADIO_RAMP_16_US = SX128X_TX_RADIO_RAMP_16_US,
TX_RADIO_RAMP_20_US = SX128X_TX_RADIO_RAMP_20_US,
}sx128x_tx_ramp_times_t;

/**
 * @brief   LoRa configuration structure.
 */
typedef struct {
  uint16_t preamble_len;                      /**< Length of preamble header */
  LoRa_spreading_factors spreading_factor;    /**< Spreading factor */ 
  LoRa_bandwidths bandwidth;                  /**< Signal bandwidth */
  LoRa_coding_rates coderate;                 /**< Error coding rate */
  uint8_t payload_length;                     /**< Default payload length */
  uint32_t frequency;                         /**< Channel frequency */
  uint8_t flags;                              /**< Boolean flags */
} sx128x_lora_settings_t;

#define SX128X_FLAG_IQ_INVERTED                 (1 << 0)
#define SX128X_FLAG_ENABLE_FIXED_HEADER_LENGTH  (1 << 2)
#define SX128X_FLAG_ENABLE_CRC                  (1 << 3)

/**
 * @brief   Radio settings.
 */
typedef struct {
    uint32_t channel;                     /*Radio channel */
    sx128x_packet_type_t packet_type;     /*Radio packet type */         
    sx128x_regulator_mode_t regulator_mode; /*Radio regulator mode */
    int8_t power;                         /*Signal power */
    sx128x_tx_ramp_times_t ramp_time;     /*ramp time */
    sx128x_rx_mode_t rx_mode;              /*RX mode */
    sx128x_lora_settings_t lora;          /*LoRa settings */
} sx128x_radio_settings_t;
#endif

// sx128x internal
#if 1
/**
 * @brief   SX128X internal data.
 */
typedef struct {
    int16_t rx_rssi;
    uint16_t rx_snr;
    uint16_t rx_length;
    rtimer_clock_t rx_timestamp;
    rtimer_clock_t receiv_timestamp;
    uint8_t packet[256];
} sx128x_internal_t;
#endif

// sx128x params
#if 1
/**
 * @brief   SX128X hardware and global parameters.
 */
typedef struct {
    spi_device_t spi;                  /**< SPI device */
    // gpio_hal_pin_t nss_pin;            /**< SPI NSS pin */
    gpio_hal_pin_t reset_pin;          /**< Reset pin */
    gpio_hal_pin_t busy_pin;           /**< Interrupt line busy */
    gpio_hal_pin_t dio1_pin;           /**< Interrupt line DIO1 (Tx done, Rx timeout) */
    gpio_hal_pin_t dio2_pin;           /**< Interrupt line DIO2 (FHSS channel change) */
    gpio_hal_pin_t dio3_pin;           /**< Interrupt line DIO3 (CAD done) */
    uint8_t paselect;                  /**< Power amplifier mode (RFO or PABOOST) */
} sx128x_params_t;
#endif

// sx128x flags
#if 1
/**
 * @brief   SX128X IRQ flags.
 */
typedef uint16_t sx128x_irq_t;
#endif

/**
 * @brief   SX128X device descriptor.
 */
typedef struct {
    sx128x_state_t state;              /**< Radio state */
    sx128x_radio_settings_t settings;  /**< Radio settings */
    sx128x_params_t params;            /**< Device driver parameters */
    sx128x_internal_t _internal;       /**< Internal sx128x data used within the driver */
    sx128x_irq_t irq;                  /**< Device IRQ flags */
} sx128x_t;

// ! getset functions
# if 1

// ! hardware
# if 1
/**
 * @brief This command fixes the base address for the packet handing operation in Tx and Rx mode for all packet types.
 * 
 * @param dev device
 * @param tx_address TX address  
 * @param rx_address RX address
 */
void sx128x_cmd_set_buffer_base_address(const sx128x_t *dev, uint8_t tx_address, uint8_t rx_address);

/**
 * @brief This command returns the length of the last received packet (payloadLengthRx) and the address of the first byte received
(rxBufferOffset), it is applicable to all modems. The address is an offset relative to the first byte of the data buffer
 * 
 * @param dev pointer to device object
 * @return (uint8_t) payload_length
 */
uint8_t sx128x_cmd_get_rx_buffer_status(sx128x_t *dev);

/**
 * @brief This command is used to enable IRQs and to route IRQs to DIO pins
 * 
 * @param dev pointer to device
 * @param dio1_mask interrupts to be enabled on DIO1
 * @param dio2_mask interrupts to be enabled on DIO2
 * @param dio3_mask interrupts to be enabled on DIO3
 */
void sx128x_cmd_set_dio_irq_params(const sx128x_t *dev, uint16_t dio1_mask, uint16_t dio2_mask, uint16_t dio3_mask);
/**
 * @brief This command returns the value of the IRQ register. 
 * 
 * @param dev pointer to device
 * @return (uint16_t) irq 16 flags over 16 bits 
 */
uint16_t sx128x_cmd_get_irq_status(const sx128x_t *dev);

/**
 * @brief This command clears an IRQ flag in IRQ register.
 * if only 1 DIO pin is used, only 1 flag is set and clearing it resets the register. Incase of multiple DIO pins used, the flags will stay high until cleared.
 * 
 * @param dev 
 * @param irq_mask bits to clear (default 0xFFFF to clear all)
 */
void sx128x_cmd_clear_irq_status(const sx128x_t *dev, uint16_t irq_mask);

/**
 * @brief Use this command to retrieve information about the last received packet. The returned parameters are frame-dependent.
 * The value returned by GetPacketStatus() command is packet-type-dependent.
 * in case of LoRa packet: [7:0] = rssiSync, [15:8] = snr.
 * 
 * @param dev pointer to device object
 */
void sx128x_cmd_get_packet_status(sx128x_t *dev);

/**
 * @brief This function is used to write the data payload to be transmitted. 
 * The address is auto-incremented, when the address exceeds 255 it wraps back to 0 due to the circular nature of data buffer. 
 * The address starts from the offset given as a parameter of the function.
 * 
 * @param dev pointer to device object
 * @param buf pointer to buffer containing the payload
 * @param len payload length
 */
void sx128x_cmd_write_buffer(const sx128x_t *dev, uint8_t *buf, size_t len);







/**
 * @brief set the event of the device. 
 * 
 * @param dev pointer to the device object
 * @param event[possibilities:, SX128X_NO_EVENT, SX128X_RX_DONE, SX128X_TX_DONE, SX128X_RX_TIMEOUT, SX128X_TX_TIMEOUT, SX128X_CAD_DONE, SX128X_CAD_DETECTED]
 */
void sx128x_set_state_event(sx128x_t *dev, sx128x_event_state_t event);

/**
 * @brief Get current event of the device.
 * 
 * @return ::sx128x_event_state_t 
 */
uint8_t sx128x_get_state_event(const sx128x_t *dev);

/**
 * @brief get custom state of rx.
 * 
 * @param dev pointer to the device object
 * @return uint8_t state from enum ::sx128x_rx_state_t
 */
uint8_t sx128x_get_state_rx(const sx128x_t *dev);














/**
 * @brief Sets the packet type for the SX128x device.
 *
 * @param dev The SX128x device.
 * @param packet_type The packet type to set.
 *                    Only SX128X_PACKET_TYPE_LORA is supported.
 */
void sx128x_cmd_set_packet_type(sx128x_t *dev, sx128x_packet_type_t packet_type);

/**
 * @brief Gets the packet type for the SX128x device.
 *
 * @param dev The SX128x device.
 * @return The packet type.
 */
uint8_t sx128x_cmd_get_packet_type(const sx128x_t *dev);
#endif
#endif


/**
 * @brief   Hardware IO IRQ callback function definition.
 * ! this function is probably
 */
typedef void (sx128x_dio_irq_handler_t)(sx128x_t *dev);

/**
 * @brief   Setup the SX128X
 *
 * @param[in] dev                      Device descriptor
 * @param[in] params                   Parameters for device initialization
 * @param[in] index                    Index of @p params in a global parameter struct array.
 *                                     If initialized manually, pass a unique identifier instead.
 */
void sx128x_setup(sx128x_t *dev, const sx128x_params_t *params, uint8_t index);

/**
 * @brief   Resets the SX128X
 *
 * @param[in] dev                      The sx128x device descriptor
 */
int sx128x_reset(const sx128x_t *dev);

/**
 * @brief   Initializes the transceiver.
 *
 * @param[in] dev                      The sx128x device descriptor
 *
 * @return result of initialization
 */
int sx128x_init(sx128x_t *dev);

/**
 * @brief   Initialize radio settings with default values
 *
 * @param[in] dev                      The sx128x device pointer
 */
void sx128x_init_radio_settings(sx128x_t *dev);

/**
 * @brief   Generates 32 bits random value based on the RSSI readings
 *
 * @attention This function sets the radio in LoRa mode and disables all
 *            interrupts from it. After calling this function either
 *            sx128x_set_rx_config or sx128x_set_tx_config functions must
 *            be called.static void sx128x_set_state_rx(sx128x_t *dev, 
 * @param[in] dev                      The sx128x device structure pointer
 *
 * @return random 32 bits value
 */
uint32_t sx128x_random(sx128x_t *dev);

/**
 * @brief   Start a channel activity detection.
 *
 * @param[in] dev                      The sx128x device descriptor
 */
void sx128x_start_cad(sx128x_t *dev);

/**
 * @brief   Checks that channel is free with specified RSSI threshold.
 *
 * @param[in] dev                      The sx128x device structure pointer
 * @param[in] freq                     channel RF frequency
 * @param[in] rssi_threshold           RSSI threshold
 *
 * @return true if channel is free, false otherwise
 */
bool sx128x_is_channel_free(sx128x_t *dev, uint32_t freq, int16_t rssi_threshold);

/**
 * @brief 
 * 
 */
 /* @brief   Reads the current RSSI value.
 *
 * @param[in] dev                      The sx128x device structure pointer
 *
 * @return the current value of RSSI (in dBm)
 */
int16_t sx128x_read_rssi(const sx128x_t *dev);

/**
 * @brief   Gets current state of transceiver.
 *
 * @param[in] dev                      The sx128x device descriptor
 *
 * @return radio state [RF_IDLE, RF_RX_RUNNING, RF_TX_RUNNING]
 */
uint8_t sx128x_get_state(const sx128x_t *dev);

uint16_t sx128x_get_firmware_version(const sx128x_t *dev);

uint8_t sx128x_cmd_get_status(const sx128x_t *dev);


/**
 */
void sx128x_cmd_set_sleep(const sx128x_t *dev, uint8_t config);
void sx128x_cmd_set_standby(const sx128x_t *dev, uint8_t config);
void sx128x_cmd_set_auto_tx(const sx128x_t *dev, uint16_t time);
void sx128x_cmd_set_tx(const sx128x_t *dev, uint8_t period_base, uint16_t period_base_count);
void _sx128x_cmd_set_rx(sx128x_t *dev, uint8_t period_base, uint16_t period_base_count);
void sx128x_cmd_set_rx_duty_cycle(const sx128x_t *dev, uint8_t period_base, uint16_t period_base_count, uint16_t sleep_period_base_count);
void sx128x_cmd_set_cad(const sx128x_t *dev);
void sx128x_set_state_rx(sx128x_t *dev, sx128x_rx_state_t rx);

void sx128x_cmd_set_packet_type(sx128x_t *dev, uint8_t packet_type);
uint8_t sx128x_cmd_get_packet_type(const sx128x_t *dev);
void sx128x_cmd_set_regulator_mode(sx128x_t *dev, uint8_t mode);
uint32_t sx128x_get_frequency(const sx128x_t *dev);
void sx128x_cmd_set_frequency(sx128x_t *dev, uint32_t freq);
void sx128x_cmd_set_tx_params(sx128x_t *dev, int8_t power, sx128x_tx_ramp_times_t ramp_time);
void sx128x_cmd_set_cad_params(const sx128x_t *dev, uint8_t symbol_num);

void sx128x_cmd_set_modulation_params(sx128x_t *dev, uint8_t param1, uint8_t param2, uint8_t param3);
void sx128x_cmd_set_packet_params(sx128x_t *dev, uint8_t param1, uint8_t param2, uint8_t param3, uint8_t param4, uint8_t param5, uint8_t param6, uint8_t param7);

uint8_t sx128x_cmd_get_rssi_inst(const sx128x_t *dev);


/**
 * @brief   Configures the radio with the given packet_type.
 *
 * @param[in] dev                      The sx128x device descriptor
 * @param[in] modem                    Modem to be used [0: FSK, 1: LoRa]
 */
void sx128x_set_modem(sx128x_t *dev, uint8_t packet_type);

/**
 * @brief   Gets the synchronization word.
 *
 * @param[in] dev                      The sx128x device descriptor
 *
 * @return The synchronization word
 */
uint8_t sx128x_get_syncword(const sx128x_t *dev);

/**
 * @brief   Sets the synchronization word.
 *
 * @param[in] dev                     The sx128x device descriptor
 * @param[in] syncword                The synchronization word
 */
void sx128x_set_syncword(sx128x_t *dev, uint8_t syncword);

/**
 * @brief   Gets the channel RF frequency.
 *
 * @param[in]  dev                     The sx128x device descriptor
 *
 * @return The channel frequency
 */
uint32_t sx128x_get_channel(const sx128x_t *dev);

/**
 * @brief   Sets the channel RF frequency.
 *
 * @param[in] dev                      The sx128x device descriptor
 * @param[in] freq                     Channel RF frequency
 */
void sx128x_set_channel(sx128x_t *dev, uint32_t freq);

/**
 * @brief   Computes the packet time on air in milliseconds.
 *
 * @pre     Can only be called if sx128x_init_radio_settings has already
 *          been called.
 *
 * @param[in] dev                      The sx128x device descriptor
 * @param[in] pkt_len                  The received packet payload length
 *
 * @return computed air time (ms) for the given packet payload length
 */
uint32_t sx128x_get_time_on_air(const sx128x_t *dev, uint8_t pkt_len);

/**
 * @brief   Sets the radio in sleep mode
 *
 * @param[in] dev                      The sx128x device descriptor
 */
void sx128x_set_sleep(sx128x_t *dev);

/**
 * @brief   Sets the radio in stand-by mode
 *
 * @param[in] dev                      The sx128x device descriptor
 */
void sx128x_set_standby(sx128x_t *dev);

/**
 * @brief   Sets the radio in reception mode.
 *
 * @param[in] dev                      The sx128x device descriptor
 */
void sx128x_set_rx(sx128x_t *dev);

/**
 * @brief   Sets the radio in transmission mode.
 *
 * @param[in] dev                      The sx128x device descriptor
 */
void sx128x_set_tx(sx128x_t *dev);

/**
 * @brief   Gets the maximum payload length.
 *
 * @param[in] dev                      The sx128x device descriptor
 *
 * @return The maximum payload length
 */
uint8_t sx128x_get_max_payload_len(const sx128x_t *dev);

/**
 * @brief   Sets the maximum payload length.
 *
 * @param[in] dev                      The sx128x device descriptor
 * @param[in] maxlen                   Maximum payload length in bytes
 */
void sx128x_set_max_payload_len(const sx128x_t *dev, uint8_t maxlen);

/**
 * @brief   Gets the SX128X operating mode
 *
 * @param[in] dev                      The sx128x device descriptor
 *
 * @return The actual operating mode
 */
uint8_t sx128x_get_state_opmode(const sx128x_t *dev);

/**
 * @brief   Sets the SX128X operating mode
 *
 * @param[in] dev                      The sx128x device descriptor
 * @param[in] op_mode                  The new operating mode
 */
void sx128x_set_state_opmode(sx128x_t *dev, uint8_t op_mode);

/**
 * @brief   Gets the SX128X bandwidth
 *
 * @param[in] dev                      The sx128x device descriptor
 *
 * @return the bandwidth
 */
uint8_t sx128x_get_bandwidth(const sx128x_t *dev);

/**
 * @brief   Sets the SX128X bandwidth
 *
 * @param[in] dev                      The sx128x device descriptor
 * @param[in] bandwidth                The new bandwidth
 */
void sx128x_set_bandwidth(sx128x_t *dev, uint8_t bandwidth);

/**
 * @brief   Gets the SX128X LoRa spreading factor
 *
 * @param[in] dev                      The sx128x device descriptor
 *
 * @return the spreading factor
 */
uint8_t sx128x_get_spreading_factor(const sx128x_t *dev);

/**
 * @brief   Sets the SX128X LoRa spreading factor
 *
 * @param[in] dev                      The sx128x device descriptor
 * @param[in] sf                       The spreading factor
 */
void sx128x_set_spreading_factor(sx128x_t *dev, uint8_t sf);

/**
 * @brief   Gets the SX128X LoRa coding rate
 *
 * @param[in] dev                      The sx128x device descriptor
 *
 * @return the current LoRa coding rate
 */
uint8_t sx128x_get_coding_rate(const sx128x_t *dev);

/**
 * @brief   Sets the SX128X LoRa coding rate
 *
 * @param[in] dev                      The sx128x device descriptor
 * @param[in] coderate                 The LoRa coding rate
 */
void sx128x_set_coding_rate(sx128x_t *dev, uint8_t coderate);


/**
 * @brief   Enable/disable the SX128X LoRa RX single mode
 *
 * @param[in] dev                      The sx128x device descriptor
 * @param[in] single                   The LoRa RX single mode
 */
void sx128x_set_rx_single(sx128x_t *dev, bool single);

/**
 * @brief   Checks if the SX128X CRC verification mode is enabled
 *
 * @param[in] dev                      The sx128x device descriptor
 *
 * @return the LoRa single mode
 */
bool sx128x_get_crc(const sx128x_t *dev);

/**
 * @brief   Enable/Disable the SX128X CRC verification mode
 *
 * @param[in] dev                      The sx128x device descriptor
 * @param[in] crc                      The CRC check mode
 */
void sx128x_set_crc(sx128x_t *dev, bool crc);

/**
 * @brief   Gets the SX128X frequency hopping period
 *
 * @param[in] dev                      The sx128x device descriptor
 *
 * @return the frequency hopping period
 */
uint8_t sx128x_get_hop_period(const sx128x_t *dev);

/**
 * @brief   Sets the SX128X frequency hopping period
 *
 * @param[in] dev                      The sx128x device descriptor
 * @param[in] hop_period               The frequency hopping period
 */
void sx128x_set_hop_period(sx128x_t *dev, uint8_t hop_period);

/**
 * @brief   Gets the SX128X LoRa fixed header length mode
 *
 * @param[in] dev                      The sx128x device descriptor
 *
 * @return the LoRa implicit mode
 */
bool sx128x_get_fixed_header_len_mode(const sx128x_t *dev);

/**
 * @brief   Sets the SX128X to fixed header length mode (explicit mode)
 *
 * @param[in] dev                      The sx128x device descriptor
 * @param[in] mode                     The header mode
 */
void sx128x_set_fixed_header_len_mode(sx128x_t *dev, bool mode);

/**
 * @brief   Gets the SX128X payload length
 *
 * @param[in] dev                      The sx128x device descriptor
 *
 * @return the payload length
 */
uint8_t sx128x_get_payload_length(const sx128x_t *dev);

/**
 * @brief   Sets the SX128X payload length
 *
 * @param[in] dev                      The sx128x device descriptor
 * @param[in] len                      The payload len
 */
void sx128x_set_payload_length(sx128x_t *dev, uint8_t len);

/**
 * @brief   Gets the SX128X TX radio power
 *
 * @param[in] dev                      The sx128x device descriptor
 *
 * @return the radio power
 */
uint8_t sx128x_get_tx_power(const sx128x_t *dev);

/**
 * @brief   Sets the SX128X transmission power
 *
 * @param[in] dev                      The sx128x device descriptor
 * @param[in] power                    The TX power
 */
// void sx128x_set_tx_power(sx128x_t *dev, int8_t power);

/**
 * @brief   Gets the SX128X preamble length
 *
 * @param[in] dev                      The sx128x device descriptor
 *
 * @return the preamble length
 */
uint16_t sx128x_get_preamble_length(const sx128x_t *dev);

/**
 * @brief   Sets the SX128X LoRa preamble length
 *
 * @param[in] dev                      The sx128x device descriptor
 * @param[in] preamble                 The LoRa preamble length
 */
void sx128x_set_preamble_length(sx128x_t *dev, uint16_t preamble);

/**
 * @brief   Sets the SX128X LoRa symbol timeout
 *
 * @param[in] dev                      The sx128x device descriptor
 * @param[in] timeout                  The LoRa symbol timeout
 */
void sx128x_set_symbol_timeout(sx128x_t *dev, uint16_t timeout);

/**
 * @brief   Sets the SX128X RX timeout
 *
 * @param[in] dev                      The sx128x device descriptor
 * @param[in] timeout                  The RX timeout
 */
void sx128x_set_rx_timeout(sx128x_t *dev, uint32_t timeout);

/**
 * @brief   Sets the SX128X TX timeout
 *
 * @param[in] dev                      The sx128x device descriptor
 * @param[in] timeout                  The TX timeout
 */
void sx128x_set_tx_timeout(sx128x_t *dev, uint32_t timeout);

/**
 * @brief   Checks if the SX128X LoRa inverted IQ mode is enabled/disabled
 *
 * @param[in] dev                      The sx128x device descriptor
 *
 * @return the LoRa IQ inverted mode
 */
bool sx128x_get_iq_inverted(const sx128x_t *dev);

/**
 * @brief   Enable/disable the SX128X LoRa IQ inverted mode
 *
 * @param[in] dev                      The sx128x device descriptor
 * @param[in] iq_invert                The LoRa IQ inverted mode
 */
void sx128x_set_iq_inverted(sx128x_t *dev, bool iq_invert);

/**
 * @brief   Sets the SX128X LoRa frequency hopping mode
 *
 * @param[in] dev                      The sx128x device descriptor
 * @param[in] freq_hop_on              The LoRa frequency hopping mode
 */
void sx128x_set_freq_hop(sx128x_t *dev, bool freq_hop_on);

/**
 * @brief   Sets the SX128X in LoRa CAD mode
 *
 * @param[in] dev                      The sx128x device descriptor
 * @param[in] symbols                  The number of symbols scanned for the CAD
 */
void sx128x_set_cad(sx128x_t *dev, uint8_t cad_symbols);

extern sx128x_t __sx128x_dev;

#ifdef SX128X_DEV_CONF
#define SX128X_DEV SX128X_DEV_CONF
#else
#define SX128X_DEV __sx128x_dev
#endif

extern const struct radio_driver sx128x_radio_driver;

#ifdef __cplusplus
}
#endif

#include "sx128x_internal.h"


#endif /* SX128X_H */
