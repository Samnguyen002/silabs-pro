/**
 * This module provides functionality for BLE clients to receive and reassemble
 * fragmented indications from a BLE server. It handles:
 * - Receiving multiple fragments in sequence
 * - Reassembling into original payload
 * - Checksum validation
 * PROTOCOL FORMAT (from server):
 * 
 * Single Fragment (payload ≤ 18 bytes):
 *   [length(1) | payload(≤18) | checksum(1)]
 * 
 * Multiple Fragments (payload > 18 bytes):
 *   Fragment 1: [length(1) | payload₁₉]
 *   Fragment 2-N: [payload₂₀]
 *   Fragment Last: [remaining_payload | checksum(1)]
 */

#ifndef BLE_DEFRAGMENT_H
#define BLE_DEFRAGMENT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define DEFRAG_MAX_PAYLOAD  200
#define QUEUE_SLOT_SIZE     30
#define QUEUE_SLOT          20

typedef enum    
{
    DEFRAG_CONTINUE = 0,    // Waiting for more fragments
    DEFRAG_COMPLETE,        // All fragemnts received
    DEFRAG_ERROR            // Error occurred
}defrag_enum_t;

void queue_init(void);

/**
 * @brief Initialize defragmentation module
 */
void defrag_init(void);

/**
 * @brief Push data fragment (received from server) into a queue/ring buffer
 */
bool defrag_push_data(const uint8_t *data, uint16_t len);

/**
 * @brief Pop the data fragment from queue and process 
 */
defrag_enum_t defrag_process_fragment(void);

/**
 * @brief Get reassembled payload (after DEFRAG_COMPLETE)
 */
bool defrag_get_payload(uint8_t **payload, uint16_t *payload_len, bool *checksum_valid);

/**
 * @brief Reset defragmentation state
 */
void defrag_reset(void);

#endif