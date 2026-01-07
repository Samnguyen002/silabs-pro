#include "sl_bt_api.h"
#include "gatt_db.h"
#include "sl_sleeptimer.h"
#include "ble_fragment_queue.h"
#include "app_iostream_usart.h"
#include "log.h"

// Global fragment queue
static fragment_queue_t frag_queue = {0};

// Init and reset the fragment queue
void fragment_queue_init(void)
{
    memset(&frag_queue, 0, sizeof(fragment_queue_t));
    frag_queue.is_sending = false;
    frag_queue.total_fragments = 0;
    frag_queue.current_fragment = 0;
}

// Prepare data for all fragments from payload and will send the first fragment
sl_status_t fragment_queue_prepare(uint8_t connection, uint16_t characteristic,
                                        uint8_t *payload, size_t payload_len)
{
    if(frag_queue.is_sending)
    {
        LOG_INFO("ERROR: Queue is busy");
        return SL_STATUS_BUSY;
    }

    if(payload_len == 0)
    {
        LOG_INFO("ERROR: Empty payload");
        return SL_STATUS_INVALID_PARAMETER;
    }

    // Calculate checksum byte
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

        LOG_INFO("CHECKSUM: %02x", frag_queue.fragments[0].data[1+payload_len]);
        return fragment_queue_send_next(connection, characteristic);
    }

    // First fragment : [length(1) | payload(max 19)]
    // Middle fragment : [payload(max 20)]
    // Last fragment : [payload(remaining) | checksum(1)]
    uint8_t frag_idx = 0;
    uint8_t offset = 0;

    // First fragments
    frag_queue.fragments[frag_idx].data[0] = payload_len;
    size_t  first_payload_size = (payload_len >= 19) ? 19 : payload_len;
    memcpy(&frag_queue.fragments[frag_idx].data[1], payload, first_payload_size);
    frag_queue.fragments[frag_idx].length = 1 + first_payload_size;
    offset += first_payload_size;
    frag_idx++;

    // Middle fragments and end fragment
    while (offset <= payload_len)
    {
        if(frag_idx >= MAX_FRAGMENTS)
        {
            LOG_INFO("ERROR: Too many fragments needed (max %d)", MAX_FRAGMENTS);
            fragment_queue_init();
            return SL_STATUS_NO_MORE_RESOURCE;
        }
        
        // Case remaining = 0 -> just pack the checksum
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
            // Middle fragment
            memcpy(&frag_queue.fragments[frag_idx].data[0], payload + offset, 20);
            frag_queue.fragments[frag_idx].length = 20;
            offset += 20;
            frag_idx++;
        }
    }
        
    frag_queue.total_fragments = frag_idx;
    frag_queue.current_fragment = 0;
    frag_queue.is_sending = true;

    LOG_INFO("Total payload: %lu bytes", payload_len);
    LOG_INFO("Total fragments: %u", frag_queue.total_fragments);
    for(int i = 0; i < frag_queue.total_fragments; i++)
    {
        LOG_INFO("  Fragment %d: %d bytes", i+1, frag_queue.fragments[i].length);
    }
    
    // Send first fragment
    return fragment_queue_send_next(connection, characteristic);
}

// Send the next fragment in queue until completing
sl_status_t fragment_queue_send_next(uint8_t connection, uint16_t characteristic)
{
    if(!frag_queue.is_sending)
    {
        LOG_INFO("ERROR: Queue is not in sending state");
        return SL_STATUS_INVALID_STATE;
    }

    if(frag_queue.current_fragment >= frag_queue.total_fragments)
    {
        LOG_INFO("ERROR: NO more fragments to send");
        return SL_STATUS_INVALID_STATE;
    }

    // Current_fragment will be increased after confirming
    uint8_t idx = frag_queue.current_fragment;
    
    LOG_INFO("Sending fragment %d/%d (%d bytes)...",
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
        LOG_INFO("ERROR: Failed to send fragment %u: 0x%04lx", idx + 1, sc);
        fragment_queue_init();  // Reset queue on error
        return sc;
    }
    
    printf("Fragment %u sent successfully, waiting for confirmation...\r\n", idx + 1);
    return SL_STATUS_OK;
}


/* If server recieve client's confirmation -> send the next fragment.
   Will be called in main loop, event change_status_id. */
void fragment_queue_on_confirmation(uint8_t connection, uint16_t characteristic)
{
    if(!frag_queue.is_sending)
    {
        LOG_INFO("Received unexpected confirmation (not sending)");
        return;
    }

    frag_queue.current_fragment++;

    if(frag_queue.current_fragment < frag_queue.total_fragments)
    {
        LOG_INFO("  Proceeding to next fragment...");
        sl_status_t sc = fragment_queue_send_next(connection, characteristic);
        if(sc != SL_STATUS_OK)
        {
            LOG_INFO("ERROR: Failed to continue sending");
            fragment_queue_init();
        }
    }
    else
    {
        LOG_INFO("\r\nALL FRAGMENTS SENT SUCCESSFULLY");
        LOG_INFO("Total: %u fragments transmitted", frag_queue.total_fragments);
        fragment_queue_init();  // Reset queue for next turn
    }
}
