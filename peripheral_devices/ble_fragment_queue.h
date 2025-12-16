/**
 * FEATURES:
 * - Automatic fragmentation of payloads up to 200 bytes
 * - Confirmation-based transmission (waits for each fragment acknowledgment)
 * - Inter-fragment delay to prevent client buffer overflow
 * - Non-blocking state machine design
 */

#ifndef BLE_FRAGMENT_QUEUE_H
#define BLE_FRAGMENT_QUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include "sl_status.h"

#define CHARAC_VALUE_LEN 20
#define MAX_FRAGMENTS    10

typedef struct 
{
    uint8_t data[CHARAC_VALUE_LEN];
    uint8_t length;
}fragment_t;

typedef struct
{
    fragment_t fragments[MAX_FRAGMENTS];    // Gather fragments
    uint8_t total_fragments;                // total of fragments will be sent
    uint8_t current_fragment;               // index of current fragment
    bool is_sending;                        // flag
}fragment_queue_t;

// Init fragment queue, init all the variables to 0
void fragment_queue_init(void);

// Prepare data for every fragment survived
sl_status_t fragment_queue_prepare(uint8_t connection, uint16_t characteristic,
                                        uint8_t *payload, size_t payload_len);

sl_status_t fragment_queue_send_next(uint8_t connection, uint16_t characteristic);

void fragment_queue_on_confirmation(uint8_t connection, uint16_t characteristic);

#endif 
