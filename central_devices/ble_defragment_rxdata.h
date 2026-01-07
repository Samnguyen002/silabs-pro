/**
 * @file ble_defragment_rxdata.h
 * @brief APIs and documentation for receiving and reassembling BLE packets
 *
 * This module provides functionality for BLE clients (Central) to receive
 * packets (fragments) sent by a BLE server (Peripheral), queue them, and
 * reassemble them into the original payload. It also validates payload
 * integrity using a checksum provided by the Peripheral.
 *
 * Implementation notes (see `ble_defragment_rxdata.c`):
 * - A small ring queue stores incoming fragments (`QUEUE_SLOT` slots of
 *   `QUEUE_SLOT_SIZE` bytes each).
 * - The first fragment contains the expected payload length in byte 0.
 * - Middle fragments carry up to 20 bytes of payload; the last fragment
 *   includes the final payload bytes followed by a checksum byte.
 * - The module exposes a small state machine: when processing fragments,
 *   the caller receives `DEFRAG_CONTINUE`, `DEFRAG_COMPLETE`, or
 *   `DEFRAG_ERROR` to indicate progress or failure.
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
} defrag_enum_t;

void queue_init(void);

/**
 * @brief Initialize the defragmentation context.
 *
 * Resets internal reassembly state (expected length, received length,
 * checksum flags, and first-fragment indicator). Call this once at startup
 * and also after a completed or failed transmission if you wish to start a
 * fresh reception.
 */
void defrag_init(void);

/**
 * @brief Push a received fragment into the internal ring queue.
 *
 * The function will copy the provided fragment into the next free queue
 * slot. Typical reasons for failure include:
 *  - `data == NULL`
 *  - `len == 0` or `len > QUEUE_SLOT_SIZE`
 *  - The ring queue is full
 *
 * @param data Pointer to the fragment bytes received from the peer
 * @param len  Number of bytes in the fragment
 * @return true on success (fragment queued), false on error
 */
bool defrag_push_data(const uint8_t *data, uint16_t len);

/**
 * @brief Pop the next queued fragment and advance the defragmentation state.
 *
 * This function reads the next fragment from the internal queue and
 * integrates it into the assembled payload. It implements the state
 * transitions described in the module header:
 *  - process first fragment (extract expected length)
 *  - append middle fragments
 *  - handle last fragment and checksum validation
 *
 * It returns:
 *  - `DEFRAG_CONTINUE` when waiting for more fragments
 *  - `DEFRAG_COMPLETE` when the full payload has been reassembled
 *  - `DEFRAG_ERROR` on protocol or processing error (length mismatch,
 *    queue underflow, empty fragment, etc.)
 *
 * @note Callers should check for `DEFRAG_COMPLETE` and then use
 *       `defrag_get_payload()` to retrieve the assembled payload and
 *       checksum validity.
 */
defrag_enum_t defrag_process_fragment(void);

/**
 * @brief Retrieve the assembled payload after completion.
 *
 * If a payload has been successfully assembled, this function writes the
 * pointer to the internal payload buffer, its length, and a boolean flag
 * indicating whether the checksum validation passed.
 *
 * The returned payload pointer references internal storage and is valid
 * until `defrag_reset()` (or the next completed transmission) is called.
 *
 * @param[out] payload       Pointer to be set to the assembled payload buffer
 * @param[out] payload_len   Pointer set to payload length in bytes
 * @param[out] checksum_valid Pointer set to true if checksum matched
 * @return true if a complete payload is available, false otherwise
 */
bool defrag_get_payload(uint8_t **payload, uint16_t *payload_len, bool *checksum_valid);

/**
 * @brief Reset defragmentation state in preparation for the next reception.
 *
 * Clears internal reassembly buffers and state so the module is ready to
 * accept a new transmission from the start.
 */
void defrag_reset(void);

#endif /* BLE_DEFRAGMENT_H */