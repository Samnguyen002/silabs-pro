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
} fragment_t;

typedef struct
{
    fragment_t fragments[MAX_FRAGMENTS];    // Gather fragments
    uint8_t total_fragments;                // Total of fragments will be sent
    uint8_t current_fragment;               // Index of current fragment during sending
    bool is_sending;                        // Flag
} fragment_queue_t;

/**
 * @brief Initialize the fragment queue.
 * 
 * Clears all buffered data, reset ths status to 'not sending', resets the fragment
 * counters. Use this during initialization or to flush/delete the queue before a new
 * transmission.
 */
void fragment_queue_init(void);

/**
 * @brief To prepare the fragment queue and start sending process.
 * 
 * Splits/divides the payload into fragments of 20bytes max, adds length to the beginning
 * and addpends checksum at the end. After preparing, the function will start sending 
 * the first fragment.
 * Structure of fragments: [length(1) | payload(max 19)] [payload(max 20)] ... [payload(remaining) | checksum(1)]
 *
 * @param[in] connection Connection handle that presents the link to the client
 * @param[in] characteristic Characteristic handle to specify where to send the fragments
 * @param[in] payload Pointer to the payload buffer 
 * @param[in] payload_len Length of the payload in bytes
 * @return sl_status_t 
 */
sl_status_t fragment_queue_prepare(uint8_t connection, uint16_t characteristic,
                                   uint8_t *payload, size_t payload_len);

/**
 * @brief Send the next fragment in queue until completing.
 * 
 * Sends the first fragment (the current pending fragment) in queue via GATT indication. The fuction will be called 
 * subsequently after obtaining a confirmation of the previous fragment.
 * 
 * @param[in] connection Connection handle that presents the link to the client
 * @param[in] characteristic Characteristic handle to specify where to send the fragments
 * @return sl_status_t 
 */
sl_status_t fragment_queue_send_next(uint8_t connection, uint16_t characteristic);

/**
 * @brief Handle confirmation from client and proceed to next fragment.
 * Examines the confirmation and sumarizes the number of fragments
 * 
 * @param[in] connection Connection handle that presents the link to the client
 * @param[in] characteristic Characteristic handle to specify where to send the fragments
 */
void fragment_queue_on_confirmation(uint8_t connection, uint16_t characteristic);

#endif 
