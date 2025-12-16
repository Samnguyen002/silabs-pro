#ifndef APP_IOSTREAM_USART_H
#define APP_IOSTREAM_USART_H

#include <stdio.h>
#include <string.h>
#include "sl_iostream.h"
#include "sl_iostream_init_instances.h"
#include "sl_iostream_handles.h"

// Initialize USART I/O stream.
void app_iostream_usart_init(void);

// USART I/O stream process action.
//void app_iostream_usart_process_action(void);

uint8_t app_iostream_checksum(uint8_t *data, uint8_t len);

#endif // APP_IOSTREAM_USART_H
