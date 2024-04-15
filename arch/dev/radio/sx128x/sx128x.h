
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
extern "C"
{
#endif

//  spi
#if 1
#ifdef SX128X_SPI_BITRATE_CONF
#define SX128X_SPI_BITRATE SX128X_SPI_BITRATE_CONF
#else
// #define SX128X_SPI_BITRATE 40000000
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
#define SX128X_BUSY_RX 0 // was 1 for testing irq, 0 to enable interrupt (receiving with interrupt not yet written)
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
  enum
  {
    SX128X_INIT_OK = 0, /**< Initialization was successful */
    SX128X_ERR_SPI,     /**< Failed to initialize SPI bus or CS line */
    SX128X_ERR_GPIOS,   /**< Failed to initialize GPIOs */
    SX128X_ERR_NODEV    /**< No valid device version found */
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
#define SX128X_TSCH_DEFAULT_TS_TIMESLOT_LENGTH                        \
  (SX128X_TSCH_DEFAULT_TS_TX_OFFSET + SX128X_TSCH_DEFAULT_TS_MAX_TX + \
   SX128X_TSCH_DEFAULT_TS_TX_ACK_DELAY + SX128X_TSCH_DEFAULT_TS_MAX_ACK)
#endif

#if 1
#define SX128X_PACKET_TYPE_DEFAULT (SX128X_PACKET_TYPE_LORA) /**< Use LoRa as default packet type */
#define SX128X_CHANNEL_DEFAULT (2400UL)                      /**< Default channel frequency, 868.3MHz (Europe) */
#define SX128X_XTAL_FREQ (32000000UL)                        /**< Internal oscillator frequency, 32MHz */
#define SX128X_RADIO_WAKEUP_TIME (1U)                        /**< In milliseconds [ms] */

#define SX128X_CRYSTAL_FREQ (52.0)
#define SX128X_DIV_EXPONANT (18)

#define SX128X_TX_TIMEOUT_DEFAULT (30 * MS_PER_SEC)     /**< TX timeout, 30s */
#define SX128X_RX_SINGLE (false)                        /**< Single byte receive mode => continuous by default */
#define SX128X_BUFFER_SIZE (256)                        /**< RX buffer size */
#define SX128X_RADIO_TX_POWER (13U)                     /**< Radio power in dBm */
#define SX128X_RADIO_TX_RAMP_TIME (TX_RADIO_RAMP_02_US) /**< Power amplifier ramp time */

#define SX128X_EVENT_HANDLER_STACK_SIZE (2048U) /**< Stack size event handler */
#endif

// sx128x state
#if 1
  typedef enum
  {
    SX128X_OPMODE_SLEEP = 0x00,
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

  typedef enum
  {
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
  typedef enum sx128x_event_state_t
  {
    SX128X_NO_EVENT = 0, /**< No event */
    SX128X_RX_DONE,      /**< Receiving complete */
    SX128X_TX_DONE,      /**< Sending complete*/
    SX128X_RX_TIMEOUT,   /**< Receiving timeout */
    SX128X_TX_TIMEOUT,   /**< Sending timeout */
    SX128X_RX_ERROR_CRC, /**< Receiving CRC error */
    SX128X_CAD_DONE,     /**< Channel activity detection complete */
    SX128X_CAD_DETECTED, /**< Channel activity detected */
  } sx128x_event_state_t;

  /**
   * @brief   Radio state.
   */
  typedef struct
  {
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
  typedef enum
  {
    SX128X_PACKET_TYPE_GFSK = 0, /**< FSK modem driver */
    SX128X_PACKET_TYPE_LORA,     /**< LoRa modem driver */
    SX128X_PACKET_TYPE_RANGING,  /**< Ranging modem driver */
    SX128X_PACKET_TYPE_FLRC,     /**< FLRC modem driver */
    SX128X_PACKET_TYPE_BLE,      /**< BLE modem driver */
  } sx128x_packet_type_t;

  /**
   * @brief   radio driver regulator mode.
   */
  typedef enum
  {
    SX128X_REGULATOR_MODE_LDO = 0x00, /*LDO regulator mode */
    SX128X_REGULATOR_MODE_DCDC = 0x01 /*DCDC regulator mode */
  } sx128x_regulator_mode_t;

  /**
   * @brief   radio driver regulator mode.
   */
  typedef enum
  {
    SX128X_RX_MODE_PERIOD = 0x00,    /*RX mode period */
    SX128X_RX_MODE_SINGLE = 0x01,    /*RX mode single */
    SX128X_RX_MODE_CONTINUOUS = 0x02 /*RX mode continuous */
  } sx128x_rx_mode_t;

  /**
   * @brief   LoRa configuration structure.
   */
  typedef struct
  {
    uint16_t preamble_len;                   /**< Length of preamble header */
    LoRa_spreading_factors spreading_factor; /**< Spreading factor */
    LoRa_bandwidths bandwidth;               /**< Signal bandwidth */
    LoRa_coding_rates coderate;              /**< Error coding rate */
    uint8_t payload_length;                  /**< Default payload length */
    uint32_t frequency;                      /**< Channel frequency */
    uint8_t flags;                           /**< Boolean flags */
  } sx128x_lora_settings_t;

#define SX128X_FLAG_IQ_INVERTED (1 << 0)
#define SX128X_FLAG_ENABLE_FIXED_HEADER_LENGTH (1 << 2)
#define SX128X_FLAG_ENABLE_CRC (1 << 3)

  typedef enum
  {
    CCA_CLEAR,
    CCA_BUSY,
    CCA_ERROR
  } CCA_STATUS;

  /**
   * @brief   Radio settings.
   */
  typedef struct
  {
    uint32_t channel;                       /*Radio channel */
    sx128x_packet_type_t packet_type;       /*Radio packet type */
    sx128x_regulator_mode_t regulator_mode; /*Radio regulator mode */
    int8_t power;                           /*Signal power */
    sx128x_tx_ramp_times_t ramp_time;       /*ramp time */
    sx128x_rx_mode_t rx_mode;               /*RX mode */
    sx128x_lora_settings_t lora;            /*LoRa settings */
  } sx128x_radio_settings_t;
#endif

// sx128x internal
#if 1
  /**
   * @brief   SX128X internal data.
   */
  typedef struct
  {
    int16_t rx_rssi;
    uint16_t rx_snr;
    uint16_t rx_length;
    uint64_t cad_timestamp;
    uint64_t rx_timestamp;
    uint64_t tx_timestamp;
    uint8_t packet[256];
  } sx128x_internal_t;
#endif

// sx128x params
#if 1
  /**
   * @brief   SX128X hardware and global parameters.
   */
  typedef struct
  {
    spi_device_t spi; /**< SPI device */
    // gpio_hal_pin_t nss_pin;            /**< SPI NSS pin */
    gpio_hal_pin_t reset_pin; /**< Reset pin */
    gpio_hal_pin_t busy_pin;  /**< Interrupt line busy */
    gpio_hal_pin_t dio1_pin;  /**< Interrupt line DIO1 (Tx done, Rx timeout) */
    gpio_hal_pin_t dio2_pin;  /**< Interrupt line DIO2 (FHSS channel change) */
    gpio_hal_pin_t dio3_pin;  /**< Interrupt line DIO3 (CAD done) */
    uint8_t paselect;         /**< Power amplifier mode (RFO or PABOOST) */
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
  typedef struct
  {
    sx128x_state_t state;             /**< Radio state */
    sx128x_radio_settings_t settings; /**< Radio settings */
    sx128x_params_t params;           /**< Device driver parameters */
    sx128x_internal_t _internal;      /**< Internal sx128x data used within the driver */
    sx128x_irq_t irq;                 /**< Device IRQ flags */
  } sx128x_t;

// ! sx128x.c functions
#if 1
  int sx128x_channel_activity_detection();
#endif

// ! getset functions
#if 1

// ! helper functions
#if 1
  /**
   * @brief return the timeout in seconds by multiplying the periodBaseCount by the correct factor based on the periodBase.
   *
   * @param periodBase hexadecimal value representing a decimal value which serves as the period for 1 tick
   * @param periodBaseCount Amount of ticks before the device times out
   * @return uint8_t
   */
  uint8_t _sx128x_get_timeout_in_s(uint8_t periodBase, uint16_t periodBaseCount);

  /**
   * @brief Convert a hexadecimal value into the representative decimal value according to the datasheet.
   *
   * @param ramp_time hexadecimal value representing a decimal value which serves as the ramp time
   * @return uint16_t
   */
  uint16_t _sx128x_get_ramp_time_in_micro_s(sx128x_tx_ramp_times_t ramp_time);
#endif

// ! hardware
#if 1
  /**
   * @brief This command fixes the base address for the packet handing operation in Tx and Rx mode for all packet types.
   * \note 11.6.6 P83
   *
   * @param dev device
   * @param tx_address TX address
   * @param rx_address RX address
   */
  void sx128x_cmd_set_buffer_base_address(const sx128x_t *dev, uint8_t tx_address, uint8_t rx_address);

  /**
   * @brief This command returns the length of the last received packet
   * payloadLengthRx) and the address of the first byte received(rxBufferOffset),
   * it is applicable to all modems. The address is an offset relative to the
   * first byte of the data buffer
   * \note 11.7.1 P86
   *
   * @param dev pointer to device object
   * @return (uint8_t) payload_length
   */
  uint8_t sx128x_cmd_get_rx_buffer_status(sx128x_t *dev);

  /**
   * @brief This command is used to enable IRQs and to route IRQs to DIO pins.
   *
   * \note 11.8.1 P90
   *
   * @param dev pointer to device
   * @param dio1_mask interrupts to be enabled on DIO1
   * @param dio2_mask interrupts to be enabled on DIO2
   * @param dio3_mask interrupts to be enabled on DIO3
   */
  void sx128x_cmd_set_dio_irq_params(const sx128x_t *dev, uint16_t dio1_mask, uint16_t dio2_mask, uint16_t dio3_mask);
  /**
   * @brief This command returns the value of the IRQ register.
   * \note 11.8.2 P91
   *
   * @param dev pointer to device
   * @return (uint16_t) irq 16 flags over 16 bits
   */
  uint16_t sx128x_cmd_get_irq_status(const sx128x_t *dev);

  /**
   * @brief This command clears an IRQ flag in IRQ register. if only 1 DIO pin is
   *  used, only 1 flag is set and clearing it resets the register. Incase of
   * multiple DIO pins used, the flags will stay high until cleared.
   * \note 11.8.3 P91
   *
   * @param dev
   * @param irq_mask bits to clear (default 0xFFFF to clear all)
   */
  void sx128x_cmd_clear_irq_status(const sx128x_t *dev, uint16_t irq_mask);

  /**
   * @brief Use this command to retrieve information about the last received
   *  packet. The returned parameters are frame-dependent.
   * The value returned by GetPacketStatus() command is packet-type-dependent.
   * in case of LoRa packet: [7:0] = rssiSync, [15:8] = snr.
   * \note 11.7.2 P87
   *
   * @param dev pointer to device object
   */
  void sx128x_cmd_get_packet_status(sx128x_t *dev);

  /**
   * @brief This function is used to write the data payload to be transmitted.
   * The address is auto-incremented, when the address exceeds 255 it wraps back
   * to 0 due to the circular nature of data buffer. The address starts from the
   * offset given as a parameter of the function.
   * \note 11.4 P69
   *
   * @param dev pointer to device object
   * @param buf pointer to buffer containing the payload
   * @param len payload length
   */
  void sx128x_cmd_write_buffer(const sx128x_t *dev, uint8_t *buf, size_t len);

  /**
   * @brief retrieve the transceiver status.
   * \note 11.2 P67
   *
   * @param dev pointer to device object
   * @return status byte
   */
  uint8_t sx128x_cmd_get_status(const sx128x_t *dev);

#endif

// ! opmodes
#if 1
  // ! TX

  /**
   * @brief The command SetTx() sets the device in Transmit mode. Clear IRQ status before using this command.
   *
   * @param dev pointer to device object
   * @param period_base hexadecimal value representing a decimal value which serves as the period for 1 tick
   * @param period_base_count amount of ticks before the device times out
   */
  void sx128x_cmd_set_tx(const sx128x_t *dev, uint8_t period_base, uint16_t period_base_count);

  // ! RX

  /**
   * \brief Configure transceiver for RX mode.
   * \brief - reset the current state event
   * \brief - reset the buffer pointers to 0
   * \brief - set correct DIO irq mask
   *
   * \param dev pointer to device object
   */
  void sx128x_configure_rx(sx128x_t *dev);

  /**
   * @brief The command SetRx() sets the device in Receiver mode. The IRQ status
   * should be cleared prior to using this command.
   * \note 11.5.5 P75
   *
   * @param dev pointer to device object
   * @param period_base hexadecimal value representing a decimal value which
   * serves as the period for 1 tick
   * @param period_base_count amount of ticks before the device times out
   */
  void _sx128x_cmd_set_rx(sx128x_t *dev, uint8_t period_base, uint16_t period_base_count);

  // ! CAD
  // ! undocumented
  void sx128x_set_cad(sx128x_t *dev, uint8_t cad_symbols);
  // ! undocumented
  void sx128x_cmd_set_cad_params(const sx128x_t *dev, uint8_t symbol_num);
  // ! undocumented
  void sx128x_cmd_set_cad(const sx128x_t *dev);
  // ! standby

  /**
   * @brief The command SetStandby() is used to set the device in either STDBY_RC
   * or STDBY_XOSC mode which are intermediate levels of power consumption.
   * In this mode, the transceiver may be configured for future RF operations.
   * After power on or application of a reset, the transceiver will enter in
   * STDBY_RC mode running with a 13 MHz RC clock.
   * \note 11.5.2 P73
   *
   * @param dev pointer to device object
   * @param config select clock while in standby mode.
   *    - 0 = STDBY_RC device running on RC 13 MHz
   *    - 1 = STDBY_XOSC device running on XTAL 52 MHz
   */
  void sx128x_cmd_set_standby(const sx128x_t *dev, uint8_t config);

  /**
   * @brief set the transceiver in standby mode.
   * @brief - set the state rx to off
   * @brief - set DIO irq mask to null
   * @brief - set opmode to standby
   *
   * @param dev pointer to device object
   */
  void sx128x_set_standby(sx128x_t *dev);

  // ! sleep

  /**
   * @brief The SetSleep() command is used to set the transceiver to Sleep mode
   * with the lowest current consumption possible. This command can be sent only
   * in STDBY mode (STDBY_RC or STDBY_XOSC). After rising edge of NSS, all blocks
   * are switched OFF except backup regulator if needed and the blocks specified
   * in sleepConfig parameter.
   * \note 11.5.1 P71
   *
   * @param dev pointer to device object
   * @param config 1 byte configuration:
   * - [7:4] = unused
   * - [1] = 0: data buffer flushed, 1: data retention
   * - [0] = 0 RAM flushed, 1: RAM retention
   */
  void sx128x_cmd_set_sleep(const sx128x_t *dev, uint8_t config);

  /**
   * @brief set the transceiver in sleep mode.
   * @brief - set the state rx to off
   * @brief - set the state event to NO_EVENT
   * @brief - set DIO irq mask to null
   * @brief - set opmode to sleep
   *
   * @param dev pointer to device object
   */
  void sx128x_set_sleep(sx128x_t *dev);

#endif

// ! DEVICE STATE
#if 1
  /**
   * @brief set the state of RX. Possible states:
   * @brief - sx128x_rx_off
   * @brief - sx128x_rx_listening
   * @brief - sx128x_rx_receiving
   * @brief - sx128x_rx_received
   * @brief - sx128x_rx_read
   *
   * @param dev pointer to the device object
   * @param rx state of the device
   */
  void sx128x_set_state_rx(sx128x_t *dev, sx128x_rx_state_t rx);

  /**
   * @brief get state of rx.
   *
   * @param dev pointer to the device.
   * @return uint8_t rx state from enum.
   * \retval sx128x_rx_off
   * \retval sx128x_rx_listening
   * \retval sx128x_rx_receiving
   * \retval sx128x_rx_received
   * \retval sx128x_rx_read
   */
  uint8_t sx128x_get_state_rx(const sx128x_t *dev);

  /**
   * @brief set opmode. possibilities:
   * @brief - SX128X_OPMODE_SLEEP
   * @brief - SX128X_OPMODE_STANDBY
   * @brief - SX128X_OPMODE_SYNTHESIZER_TX
   * @brief - SX128X_OPMODE_TX
   * @brief - SX128X_OPMODE_SYNTHESIZER_RX
   * @brief - SX128X_OPMODE_RX
   * @brief - SX128X_OPMODE_RX_SINGLE
   * @brief - SX128X_OPMODE_RX_CONTINUOUS
   * @brief - SX128X_OPMODE_RX_ACK
   * @brief - SX128X_OPMODE_CAD
   *
   * @param dev pointer to device object
   * @param op_mode opmode from enum
   */
  void sx128x_set_state_opmode(sx128x_t *dev, sx128x_opmode_t op_mode);

  /**
   * @brief get state of opmode.
   *
   * @param dev pointer to device object
   * @return uint8_t opmode from enum
   * \retval SX128X_OPMODE_SLEEP
   * \retval SX128X_OPMODE_STANDBY
   * \retval SX128X_OPMODE_SYNTHESIZER_TX
   * \retval SX128X_OPMODE_TX
   * \retval SX128X_OPMODE_SYNTHESIZER_RX
   * \retval SX128X_OPMODE_RX
   * \retval SX128X_OPMODE_RX_SINGLE
   * \retval SX128X_OPMODE_RX_CONTINUOUS
   * \retval SX128X_OPMODE_RX_ACK
   * \retval SX128X_OPMODE_CAD
   */
  uint8_t sx128x_get_state_opmode(const sx128x_t *dev);

  /**
   * @brief set the state of the current event. possibilities:
   * @brief - SX128X_NO_EVENT
   * @brief - SX128X_RX_DONE
   * @brief - SX128X_TX_DONE
   * @brief - SX128X_RX_TIMEOUT
   * @brief - SX128X_TX_TIMEOUT
   * @brief - SX128X_RX_ERROR_CRC
   * @brief - SX128X_CAD_DONE
   * @brief - SX128X_CAD_DETECTED
   *
   * @param dev pointer to the device
   * @param event event from enum
   */
  void sx128x_set_state_event(sx128x_t *dev, sx128x_event_state_t event);

  /**
   * @brief get state of the current event.
   *
   * @param dev pointer to the device
   * @return uint8_t event from enum
   * \retval SX128X_NO_EVENT
   * \retval SX128X_RX_DONE
   * \retval SX128X_TX_DONE
   * \retval SX128X_RX_TIMEOUT
   * \retval SX128X_TX_TIMEOUT
   * \retval SX128X_RX_ERROR_CRC
   * \retval SX128X_CAD_DONE
   * \retval SX128X_CAD_DETECTED
   */
  uint8_t sx128x_get_state_event(const sx128x_t *dev);
#endif

// ! device settings
#if 1
  /**
   * @brief internally set the packet type we expect to be receiving. possibilities:
   * @brief - SX128X_PACKET_TYPE_GFSK
   * @brief - SX128X_PACKET_TYPE_LORA
   * @brief - SX128X_PACKET_TYPE_RANGING
   * @brief - SX128X_PACKET_TYPE_FLRC
   * @brief - SX128X_PACKET_TYPE_BLE
   *
   * @param dev pointer to the device
   * @param packet_type type from enum
   */
  void sx128x_set_modem(sx128x_t *dev, sx128x_packet_type_t packet_type);

  /**
   * @brief The command SetPacketType() sets the transceiver radio frame out of a
   * choice of 6 different packet types. Despite some of them using the same
   * physical modem, they do not all share the same parameters.
   * possibilities:
   * @brief - SX128X_PACKET_TYPE_GFSK
   * @brief - SX128X_PACKET_TYPE_LORA
   * @brief - SX128X_PACKET_TYPE_RANGING
   * @brief - SX128X_PACKET_TYPE_FLRC
   * @brief - SX128X_PACKET_TYPE_BLE
   * \note 11.6.1 P80
   *
   * @param dev pointer to device object
   * @param packet_type type from enum
   */
  void sx128x_cmd_set_packet_type(sx128x_t *dev, sx128x_packet_type_t packet_type);

  /**
   * @brief get the packet type we expect to be receiving.
   *
   * @param dev pointer to the device
   * @return uint8_t packet type from enum
   * \retval SX128X_PACKET_TYPE_GFSK
   * \retval SX128X_PACKET_TYPE_LORA
   * \retval SX128X_PACKET_TYPE_RANGING
   * \retval SX128X_PACKET_TYPE_FLRC
   * \retval SX128X_PACKET_TYPE_BLE
   */
  uint8_t sx128x_cmd_get_packet_type(const sx128x_t *dev);

  /**
   * @brief set the regulator mode. possibilities:
   * @brief - SX128X_REGULATOR_MODE_LDO
   * @brief - SX128X_REGULATOR_MODE_DCDC
   * \note 13.7 P131
   *
   * @param dev pointer to the device
   * @param mode mode from enum
   */
  void sx128x_cmd_set_regulator_mode(sx128x_t *dev, sx128x_regulator_mode_t mode);

  /**
   * @brief This command sets the Tx output power using parameter power and the
   * Tx ramp time using parameter rampTime. This command is available for all
   * packetType
   * \note 11.6.4 P82
   *
   * @param dev pointer to the device
   * @param power value between -18 and 13 dBm (13 becomes 12.5)
   * @param ramp_time hexadecimal value representing a setting for the ramp time
   */
  void sx128x_cmd_set_tx_params(sx128x_t *dev, int8_t power, sx128x_tx_ramp_times_t ramp_time);

  /**
   * @brief get tx power from internal settings.
   *
   * @param dev pointer to device object
   * @return uint8_t : number between -18 and 13 dBm (13 becomes 12.5)
   */
  uint8_t sx128x_get_tx_power(const sx128x_t *dev);

  /**
   * @brief get ramp time from internal settings.
   * @param dev pointer to device object
   * @return uint16_t hexadecimal value representing a setting for the ramp time
   */
  sx128x_tx_ramp_times_t sx128x_get_tx_ramp_time(const sx128x_t *dev);

  // ! undocumented
  void sx128x_cmd_set_frequency(sx128x_t *dev, uint32_t freq);
  // ! undocumented
  uint32_t sx128x_get_frequency(const sx128x_t *dev);
  // ! undocumented
  void sx128x_set_rx_mode(sx128x_t *dev, sx128x_rx_mode_t rx_mode);
  // ! undocumented
  sx128x_rx_mode_t sx128x_get_rx_mode(const sx128x_t *dev);

#endif

// ! Device settings: LoRa
#if 1
  // ! undocumented
  void sx128x_set_preamble_length(sx128x_t *dev, uint16_t preamble);
  // ! undocumented
  uint16_t sx128x_get_preamble_length(const sx128x_t *dev);
  // ! undocumented
  void sx128x_set_bandwidth(sx128x_t *dev, LoRa_bandwidths bandwidth);
  // ! undocumented
  LoRa_bandwidths sx128x_get_bandwidth(const sx128x_t *dev);
  // ! undocumented
  void sx128x_set_spreading_factor(sx128x_t *dev, LoRa_spreading_factors spreading_factor);
  // ! undocumented
  LoRa_spreading_factors sx128x_get_spreading_factor(const sx128x_t *dev);
  // ! undocumented
  void sx128x_set_coding_rate(sx128x_t *dev, LoRa_coding_rates coderate);
  // ! undocumented
  LoRa_coding_rates sx128x_get_coding_rate(const sx128x_t *dev);
  // ! undocumented
  void sx128x_set_payload_length(sx128x_t *dev, uint8_t len);
  // ! undocumented
  void sx128x_cmd_set_modulation_params(sx128x_t *dev, LoRa_spreading_factors SF, LoRa_bandwidths BW, LoRa_coding_rates code_rate);
  // ! undocumented
  void sx128x_cmd_set_packet_params(sx128x_t *dev, uint8_t preamble_len, uint8_t enable_fixed_header_len, uint8_t payload_len, uint8_t enable_crc, uint8_t iq_inverted, uint8_t param6, uint8_t param7);
  // ! undocumented
  void sx128x_set_crc(sx128x_t *dev, bool crc);
  // ! undocumented
  bool sx128x_get_crc(const sx128x_t *dev);
  // ! undocumented
  void sx128x_set_fixed_header_len_mode(sx128x_t *dev, bool fixed_len);
  // ! undocumented
  bool sx128x_get_fixed_header_len_mode(const sx128x_t *dev);
  // ! undocumented
  void sx128x_set_iq_inverted(sx128x_t *dev, bool iq_invert);
  // ! undocumented
  bool sx128x_get_iq_inverted(const sx128x_t *dev);

#endif
#endif
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
