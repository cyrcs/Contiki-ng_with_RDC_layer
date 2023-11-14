#include "net/mac/csma/csma.h"
#include "net/mac/csma/csma-output.h"
#include "net/mac/mac-sequence.h"
#include "net/packetbuf.h"
#include "net/netstack.h"
#include "net/mac/rdc/rdc.h"

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "CSMA"
#define LOG_LEVEL LOG_LEVEL_MAC
/*---------------------------------------------------------------------------*/
static void
init(void)
{
  radio_value_t radio_max_payload_len;

  /* Check that the radio can correctly report its max supported payload */
  if(NETSTACK_RADIO.get_value(RADIO_CONST_MAX_PAYLOAD_LEN, &radio_max_payload_len) != RADIO_RESULT_OK) {
    LOG_ERR("! radio does not support getting RADIO_CONST_MAX_PAYLOAD_LEN. Abort init.\n");
    return;
  }

#if CSMA_SEND_SOFT_ACK
  radio_value_t radio_rx_mode;

  /* Disable radio driver's autoack */
  if(NETSTACK_RADIO.get_value(RADIO_PARAM_RX_MODE, &radio_rx_mode) != RADIO_RESULT_OK) {
    LOG_WARN("radio does not support getting RADIO_PARAM_RX_MODE\n");
  } else {
    /* Unset autoack */
    radio_rx_mode &= ~RADIO_RX_MODE_AUTOACK;
    if(NETSTACK_RADIO.set_value(RADIO_PARAM_RX_MODE, radio_rx_mode) != RADIO_RESULT_OK) {
      LOG_WARN("radio does not support setting RADIO_PARAM_RX_MODE\n");
    }
  }
#endif

  mac_sequence_init();

#if LLSEC802154_USES_AUX_HEADER
#ifdef CSMA_LLSEC_DEFAULT_KEY0
  uint8_t key[16] = CSMA_LLSEC_DEFAULT_KEY0;
  csma_security_set_key(0, key);
#endif
#endif /* LLSEC802154_USES_AUX_HEADER */
  csma_output_init();
  on();
}

/*---------------------------------------------------------------------------*/
static void init_sec(void)
{
#if LLSEC802154_USES_AUX_HEADER
  if(packetbuf_attr(PACKETBUF_ATTR_SECURITY_LEVEL) ==
     PACKETBUF_ATTR_SECURITY_LEVEL_DEFAULT) {
    packetbuf_set_attr(PACKETBUF_ATTR_SECURITY_LEVEL,
                       CSMA_LLSEC_SECURITY_LEVEL);
  }
#endif
}


/*---------------------------------------------------------------------------*/
static int
on(void)
{
  //BUG check if NETSTACK_RDC expands correctly
  return NETSTACK_RDC.on();
}

/*---------------------------------------------------------------------------*/
static int
off(int keep_radio_on) 
{
  //BUG check if NETSTACK_RDC expands correctly
  return NETSTACK_RDC.off(keep_radio_on);
}


/*---------------------------------------------------------------------------*/
const struct mac_driver csma_driver = {
  "CSMA",
  init,
  send_packet,
  input_packet,
  on,
  off,
  max_payload,
};
/*---------------------------------------------------------------------------*/



