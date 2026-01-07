#ifndef APP_IOSTREAM_USART_H
#define APP_IOSTREAM_USART_H

#include <stdio.h>
#include <string.h>
#include "sl_iostream.h"
#include "sl_iostream_init_instances.h"
#include "sl_iostream_handles.h"

/**
 * @brief Initialize USART I/O stream and retarget standard I/O to VCOM.
 *
 * Disables buffering on stdout/stdin (when using newlib/GCC) and sets the
 * default iostream handle so that printf/scanf use the VCOM interface.
 */
void app_iostream_usart_init(void);

/**
 * @brief Compute two's complement checksum of a payload.
 *
 * @param[in] data Pointer to the payload buffer
 * @param[in] Length of the payload in bytes
 * @return 8-bit checksum computed as two's complement of the sum of bytes
 */
uint8_t app_iostream_checksum(uint8_t *data, uint8_t len);

#endif 
