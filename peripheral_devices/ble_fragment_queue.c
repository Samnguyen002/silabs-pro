#include "sl_bt_api.h"
#include "gatt_db.h"
#include "sl_sleeptimer.h"
#include "ble_fragment_queue.h"
#include "app_iostream_usart.h"

// Global fragment queue
static fragment_queue_t frag_queue = {0};

/**
 * @brief Init and reset the fragment queue
 */
void fragment_queue_init(void)
{
    memset(&frag_queue, 0, sizeof(fragment_queue_t));
    frag_queue.is_sending = false;
    frag_queue.total_fragments = 0;
    frag_queue.current_fragment = 0;
}

/**
 * @brief prepare data for all fragments from payload and will send the first fragment
 */
sl_status_t fragment_queue_prepare(uint8_t connection, uint16_t characteristic,
                                        uint8_t *payload, size_t payload_len)
{
    if(frag_queue.is_sending)
    {
        printf("ERROR: Queue is busy\r\n");
        return SL_STATUS_BUSY;
    }

    if(payload_len == 0)
    {
        printf("ERROR: Empty payload\r\n");
        return SL_STATUS_INVALID_PARAMETER;
    }

    // calculate checksum byte
    uint8_t checksum = app_iostream_checksum(payload, payload_len);

    // This case indicates the payload <= max value length of characteristic - 2
    if(payload_len <= 18)
    {
        frag_queue.fragments[0].data[0] = payload_len;
        memcpy(&frag_queue.fragments[0].data[1], payload, payload_len);
        frag_queue.fragments[0].data[1+payload_len] = checksum;
        frag_queue.fragments[0].length = 1 + payload_len + 1;
        frag_queue.total_fragments = 1;
        frag_queue.is_sending = true;

        printf(">Checksum: %02x\r\n", frag_queue.fragments[0].data[1+payload_len]);
        return fragment_queue_send_next(connection, characteristic);
    }

    // first fragment : [length(1) | payload(max 19)]
    // middle fragment : [payload(max 20)]
    // last fragment : [payload(remaining) | checksum(1)]
    uint8_t frag_idx = 0;
    uint8_t offset = 0;

    // first fragments
    frag_queue.fragments[frag_idx].data[0] = payload_len;
    size_t  first_payload_size = (payload_len >= 19) ? 19 : payload_len;
    memcpy(&frag_queue.fragments[frag_idx].data[1], payload, first_payload_size);
    frag_queue.fragments[frag_idx].length = 1 + first_payload_size;
    offset += first_payload_size;
    frag_idx++;

    // middle fragments and end fragment
    while (offset <= payload_len)
    {
        if(frag_idx >= MAX_FRAGMENTS)
        {
            printf("ERROR: Too many fragments needed (max %d)\r\n", MAX_FRAGMENTS);
            fragment_queue_init();
            return SL_STATUS_NO_MORE_RESOURCE;
        }
        
        // case remaining = 0 -> just pack the checksum
        size_t remaining = payload_len - offset;
        
        // If last fragment is under <= 19 bytes
        if(remaining <= 19)
        {
            // Fragment cuối: [remaining_payload | checksum]
            memcpy(&frag_queue.fragments[frag_idx].data[0], payload + offset, remaining);
            frag_queue.fragments[frag_idx].data[remaining] = checksum;
            frag_queue.fragments[frag_idx].length = remaining + 1;
            offset += remaining;
            frag_idx++;
            break;
        }
        else
        {
            // middle fragment
            memcpy(&frag_queue.fragments[frag_idx].data[0], payload + offset, 20);
            frag_queue.fragments[frag_idx].length = 20;
            offset += 20;
            frag_idx++;
        }
    }
        
    frag_queue.total_fragments = frag_idx;
    frag_queue.current_fragment = 0;
    frag_queue.is_sending = true;

    printf("Total payload: %d bytes\r\n", (int)payload_len);
    printf("Total fragments: %d\r\n", frag_queue.total_fragments);
    for(int i = 0; i < frag_queue.total_fragments; i++)
    {
        printf("  Fragment %d: %d bytes\r\n", i+1, frag_queue.fragments[i].length);
    }
    printf("---------------------------------\r\n\n");
    
    // send first fragment
    return fragment_queue_send_next(connection, characteristic);
}

/**
 * @brief Send the next fragment in queue until completeing
 */
sl_status_t fragment_queue_send_next(uint8_t connection, uint16_t characteristic)
{
    if(!frag_queue.is_sending)
    {
        printf("ERROR: Queue is not in sending state\r\n");
        return SL_STATUS_INVALID_STATE;
    }

    if(frag_queue.current_fragment >= frag_queue.total_fragments)
    {
        printf("ERROR: NO more fragments to send\r\n");
        return SL_STATUS_INVALID_STATE;
    }

    // current_fragment will be increased after confirming
    uint8_t idx = frag_queue.current_fragment;
    
    printf("->Sending fragment %d/%d (%d bytes)...\r\n",
            idx + 1,
            frag_queue.total_fragments,
            frag_queue.fragments[idx].length);
    
    sl_status_t sc = sl_bt_gatt_server_send_indication(
                        connection,
                        characteristic,
                        frag_queue.fragments[idx].length,
                        frag_queue.fragments[idx].data);
    
    if(sc != SL_STATUS_OK)
    {
        printf("ERROR: Failed to send fragment %d: 0x%04lx\r\n", idx + 1, sc);
        fragment_queue_init();  // Reset queue on error
        return sc;
    }
    
    printf("Fragment %d sent successfully, waiting for confirmation...\r\n", idx + 1);
    return SL_STATUS_OK;
}

/**
 * @brief If server recieve client's confirmation -> send the next fragment.
 * Will be called in main loop, event change_status_id.
 */
void fragment_queue_on_confirmation(uint8_t connection, uint16_t characteristic)
{
    if(!frag_queue.is_sending)
    {
        printf("Received unexpected confirmation (not sending)\r\n");
        return;
    }

    frag_queue.current_fragment++;

    if(frag_queue.current_fragment < frag_queue.total_fragments)
    {
        printf("Proceeding to next fragment...\r\n\n");
        sl_status_t sc = fragment_queue_send_next(connection, characteristic);
        if(sc != SL_STATUS_OK)
        {
            printf("ERROR: Failed to continue sending\e\n");
            fragment_queue_init();
        }
    }
    else
    {
        printf("\r\nALL FRAGMENTS SENT SUCCESSFULLY\r\n");
        printf("Total: %d fragments transmitted\r\n\n", frag_queue.total_fragments);
        fragment_queue_init();  // Reset queue for next turn
    }
}