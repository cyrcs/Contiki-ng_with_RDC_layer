
/**
 * \file
 *         A MAC protocol implementation that does not do anything.
 * \author
 *         Adam Dunkels <adam@sics.se>
 * Changes made by
 *          Louka Grignard <louka.michael.grignard@vub.be>
 */

#ifndef NULLRDC_NOFRAMER_H_
#define NULLRDC_NOFRAMER_H_

#include "net/mac/rdc/rdc.h"
#include "dev/radio.h"

extern const struct rdc_driver nullrdc_noframer_driver;

#endif /* NULLRDC_NOFRAMER_H_ */
