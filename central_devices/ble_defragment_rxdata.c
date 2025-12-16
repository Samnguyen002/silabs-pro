#include "ble_defragment_rxdata.h"
#include "app_iostream_usart.h"

// Define a node of the queue
typedef struct 
{
    uint8_t data[QUEUE_SLOT_SIZE];
    uint16_t len;
}queue_slot_t;

// Define the context of fragments in one transmission
typedef struct 
{
    uint8_t complete_buffer[DEFRAG_MAX_PAYLOAD];
    uint16_t expected_len;                          // [NOTE]: This length only contains length of real string (payload)
    uint16_t received_len;                          
    uint8_t received_checksum;                      // From last fragment
    bool checksum_valid;
    bool is_first_fragment;
    bool is_complete;
}defrag_context_t;

static queue_slot_t queue[QUEUE_SLOT];
static defrag_context_t defrag_cxt = {0};
static volatile uint8_t q_head = 0;         // head: index to write
static volatile uint8_t q_tail = 0;         // tail: index to read

/*************************
 **** LOCAL FUCNTIONS ****
 *************************/

// Return the next index of queue (ring buffer)
static uint8_t next_queue_index(uint8_t i)
{
    return (uint8_t)(i + 1)%QUEUE_SLOT;
}

static defrag_enum_t process_first_fragment(uint8_t *data, uint16_t len)
{
    if(len < 2)
    {
        printf(">First fragment too short\r\n");
        return DEFRAG_ERROR;
    }

    // First byte is the payload length
    defrag_cxt.expected_len = data[0];

    printf("-----Defragmentation Started-----\r\n");
    printf(">Expected length: %d byte\r\n", (int)defrag_cxt.expected_len);

    if(defrag_cxt.expected_len == 0 || defrag_cxt.expected_len > DEFRAG_MAX_PAYLOAD)
    {
        printf(">Invalid length\r\n");
        return DEFRAG_ERROR;
    }

    // Check length if it's a single fragment
    if(len == 1 + defrag_cxt.expected_len + 1)
    {
        printf("SINGLE FRAGMENT\r\n");
        memcpy(defrag_cxt.complete_buffer, &data[1], defrag_cxt.expected_len);
        defrag_cxt.received_len = defrag_cxt.expected_len;
        defrag_cxt.received_checksum = data[len - 1];

        // Validate checksum byte
        uint8_t temporary_checksum = app_iostream_checksum(defrag_cxt.complete_buffer, 
                                                           defrag_cxt.expected_len);

        printf("received checksum: %02x and cal_checksum: %02x\r\n", 
                    defrag_cxt.received_checksum, 
                    temporary_checksum);
        if(temporary_checksum == defrag_cxt.received_checksum)
        {
            printf("[CHECKSUM] payload not LOST\r\n");
            defrag_cxt.checksum_valid = true;
        }
        else
        {
            printf("[CHECKSUM] payload LOST\r\n");
        }

        defrag_cxt.complete_buffer[defrag_cxt.expected_len] = '\0'; 
        printf("[RECEIVED] Data: %s, len %d\r\n", (char *)defrag_cxt.complete_buffer, 
                                              (int)(defrag_cxt.expected_len));
        defrag_cxt.is_complete = true;
        return DEFRAG_COMPLETE;
    }

    // Multiple fragments [length | first_19_bytes]
    uint16_t first_payload_len = len - 1;
    memcpy(defrag_cxt.complete_buffer, &data[1], first_payload_len);
    defrag_cxt.received_len = first_payload_len;
    defrag_cxt.is_first_fragment = false;

    defrag_cxt.complete_buffer[first_payload_len] = '\0'; 
    printf("[FRAGMENT 1] Data: %s, len: %d\r\n", (char *)defrag_cxt.complete_buffer, 
                                             (int)first_payload_len);
    
    return DEFRAG_CONTINUE;
}

static defrag_enum_t process_subsequent_fragment(uint8_t *data, uint16_t len)
{
    uint8_t buffer[20];         // Just is temporary buffer

    // Dealed with the first fragment
    if(len == 0)
    {
        printf(">Empty fragment\r\n");
        return DEFRAG_ERROR;
    }

    uint16_t remaining = defrag_cxt.expected_len - defrag_cxt.received_len;
    printf(" remaining len: %d and fragment_len: %d\r\n", (int)remaining,(int)len);

    // Check if last fragment: [remaining/checksum]
    if(remaining + 1 <= 20)
    {
        uint8_t temporary_checksum;
        uint16_t payload_len = len - 1;

        if(payload_len != remaining)
        {
            printf(">Last fragment size mismatch\r\n");
            return DEFRAG_ERROR;
        }

        // Coppy remaining payload
        memcpy(&defrag_cxt.complete_buffer[defrag_cxt.received_len], data, payload_len);
        defrag_cxt.received_len += payload_len;

        // Validate checksum byte
        defrag_cxt.received_checksum = data[len-1];
        temporary_checksum = app_iostream_checksum(defrag_cxt.complete_buffer, 
                                                   defrag_cxt.expected_len);
        if(temporary_checksum == defrag_cxt.received_checksum)
        {
            printf("[CHECKSUM] payload NOT LOST , in subsequent fragment\r\n");
            defrag_cxt.checksum_valid = true;
        }
        else
        {
            printf("[CHECKSUM] payload LOST, in subsequent fragment\r\n ");
        }

        memcpy(buffer, data, payload_len);
        buffer[payload_len] = '\0';
        printf("[LAST FRAGMENT] Data: %s, len: %d\r\n", (char *)buffer, 
                                                        (int)payload_len);

        defrag_cxt.complete_buffer[defrag_cxt.received_len] = '\0';                                            
        printf("[TOTAL FRAGMENT] Data: %s, len: %d\r\n", (char *)defrag_cxt.complete_buffer, 
                                                         (int)defrag_cxt.received_len);

        defrag_cxt.is_complete = true;
        return DEFRAG_COMPLETE;
    }
    else
    {
        // Middle fragment: [payload_20_bytes]
        if(len > remaining)
        {
            printf("> Middle fragment too larger\r\n");
            return DEFRAG_ERROR;
        }

        memcpy(&defrag_cxt.complete_buffer[defrag_cxt.received_len], data, len);
        defrag_cxt.received_len += len;

        memcpy(buffer, data, len);
        buffer[len] = '\0';
        printf("[MID FRAGMENT] Data: %s, len: %d\r\n", (char *)buffer, 
                                                       (int)len);
        
        defrag_cxt.complete_buffer[defrag_cxt.received_len] = '\0';                                            
        printf("[CUR FRAGMENT] Data: %s, len: %d\r\n", (char *)defrag_cxt.complete_buffer, 
                                                       (int)defrag_cxt.received_len);     
        
        return DEFRAG_CONTINUE;
    }
}

/*************************
 **** GLOBAL FUCNTIONS ***
 *************************/

void queue_init(void)
{
    memset(queue, 0, sizeof(queue));
    printf("[INIT] Initialize queue\r\n");
}

void defrag_init(void)
{
    memset(&defrag_cxt, 0, sizeof(defrag_context_t));
    defrag_cxt.is_first_fragment = true;
    defrag_cxt.is_complete = false;
    printf("[INIT] Initialize context\r\n");
}

void defrag_reset(void)
{
    memset(&defrag_cxt, 0, sizeof(defrag_context_t));
    defrag_cxt.is_first_fragment = true;
    defrag_cxt.is_complete = false;
    printf("[RESET] Initialize context\r\n");
}


bool defrag_push_data(const uint8_t *data, uint16_t len)
{
    if(data == NULL || len == 0 || len > QUEUE_SLOT_SIZE)
    {
        printf(">FAILED to push data #1");
        return false;
    }

    uint8_t next_idx = next_queue_index(q_head);
    if(next_idx == q_tail)
    {
        printf(">QUEUE is FULL\r\n");
        return false;
    }

    memcpy(queue[q_head].data, data, len);
    queue[q_head].len = len;

    printf("[PUSH] data: ");
    for (int i = 0; i < len; i++)
        printf("%02x", queue[q_head].data[i]);
    printf(", len: %d\r\n", queue[q_head].len);

    // Move to the next index
    q_head = next_idx;
    return true;
}

defrag_enum_t defrag_process_fragment(void)
{
    if(q_head == q_tail)
    {
        // This case occurs when server indicate slower then sl_bt_on_event occurs
        // so at that time, sl_bt_on_event() check evt and not see any events in its queue
        printf("QUEUE is EMPTY\r\n");       
        return DEFRAG_CONTINUE; 
    }

    uint16_t len = queue[q_tail].len;
    uint8_t *data = queue[q_tail].data;

    printf("[POP] data: ");
    for (int i = 0; i < len; i++)
        printf("%02x", queue[q_tail].data[i]);
    printf(", len: %d\r\n", queue[q_tail].len);

    q_tail = next_queue_index(q_tail);

    if(data == NULL || len == 0)
    {
         printf(">Invalid fragment\r\n");
        return DEFRAG_ERROR;
    }

    if(defrag_cxt.is_first_fragment)
    {
        return process_first_fragment(data, len);
    }
    else
    {
        return process_subsequent_fragment(data, len);
    }
}

bool defrag_get_payload(uint8_t **payload, uint16_t *payload_len, bool *checksum_valid)
{
  if (!defrag_cxt.is_complete)
  {
    return false;
  }
  
  if (payload != NULL)
  {
    *payload = defrag_cxt.complete_buffer;
  }
  
  if (payload_len != NULL)
  {
    *payload_len = defrag_cxt.received_len;
  }
  
  if (checksum_valid != NULL)
  {
    *checksum_valid = defrag_cxt.checksum_valid;
  }
  
  return true;
}