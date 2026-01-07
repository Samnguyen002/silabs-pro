/***************************************************************************//**
 * @file
 * @brief Core application logic.
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
#include "sl_status.h"
#include "sl_bt_api.h"
#include "sl_main_init.h"
#include "app_assert.h"
#include "gatt_db.h"
#include "app.h"
#include "sl_sleeptimer.h"

#include "burtc.h"
#include "app_iostream_usart.h"
#include "ble_fragment_queue.h"
#include "app_button_pairing_complete.h"

#include "sl_board_control.h"
#include "dmd.h"
#include "glib.h"

#ifndef DELAY_MS
#define DELAY_MS 2000
#endif

#ifndef BUFSIZE
#define BUFSIZE    80
#endif

#define DISPLAYONLY       0
#define DISPLAYYESNO      1
#define KEYBOARDONLY      2
#define NOINPUTNOOUTPUT   3
#define KEYBOARDDISPLAY   4

// MITM.. a bitmask of multiple flags (read in sl_bt_api.h)
//// JUSTWORK
// #define MITM_PROTECTION      (0x00)              // 0=JustWorks, 1=PasskeyEntry or NumericComparison
// #define IO_CAPABILITY        (NOINPUTNOOUTPUT)

// // PASSKEY, central have keyboard
// #define MITM_PROTECTION      (0x01)
// #define IO_CAPABILITY        (DISPLAYONLY)

// Numeric Comparison
#define MITM_PROTECTION      (0x01)
#define IO_CAPABILITY        (DISPLAYYESNO)     // KEYBOARDDISPLAY

#ifndef IO_CAPABILITY 
#error "Must define which IO capability device supports\r\n"
#endif

// Relate to display 
#define X_BORDER    0
#define Y_BORDER    0
#define SCREEN_REFRESH_PERIOD   (32768/4)

typedef enum
{
  IDLE,
  DISPLAY_PASSKEY,
  PROMPT_YESNO,
  PROMPT_CONFIRM_PASSKEY,
  BOND_SUCCESS,
  BOND_FAILURE
}pair_state_t;

typedef enum
{
  INDICATION_DISABLE,
  INDICATION_ENABLE,
  INDICATION_CONFIRM
}ind_state_t;

// [DISPLAY] Default strings and context used to show role/passkey on the LCD display
static char role_display_string[] = "   RESPONDER   ";
static char passkey_display_string[] = "00000000000000";
static uint32_t xOffset, yOffset;
static GLIB_Context_t glibContext;

static volatile uint32_t passkey = 0;
static volatile pair_state_t state = IDLE;

uint8_t adv_payload[] = {
  // Flags (3 bytes)
  2, 0x01, 0x06,
  // Service UUIDs 
  17, 0x07, 0x40, 0x30, 0x57, 0x13, 0x72, 0xd9, 0x62, 0x83, 0xdf, 0x4c, 0xb8, 0x80, 0xd9, 0x81, 0x7d, 0x46
};

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;
static uint8_t connection_handle = 0xff;

// Variables to hold BURTC count and converted time in seconds.
static uint32_t count;
static uint32_t time;

// Variable for creating a non-blocking delay and state
sl_sleeptimer_timer_handle_t timer_handle;
volatile bool advertising = false;
volatile bool notification = false;
static ind_state_t ind_state = INDICATION_DISABLE;

// Periodic timer callback
static void timer_handler(sl_sleeptimer_timer_handle_t *handle, void *data);

// Send data of notification.
static sl_status_t send_current_time_notification(void);
sl_status_t send_usart_packet_over_ble(uint8_t *payload, size_t payload_len);

// Reading data fucntion
size_t read_line_from_iostream(sl_iostream_t *handle, uint8_t *out_buf, 
                               size_t max_len, uint32_t timeout);

// PASSKEY
#if (IO_CAPABILITY != KEYBOARDONLY)
static uint32_t make_passkey_from_address(bd_addr address);
#endif
void graphics_init(void);
void graphics_clear(void);
void graphics_update(void);
void graphics_AppendString(char *str);
void print_empty_line(uint8_t n_line);
// Depends on pair_state_t to display any string (passkey, bonding state, ...)
void refresh_display(void);   // will be called in timer callback or loopback

// Callbacks function for button events
void button_event_handler(const button_event_t *evt);

// Application Init.
void app_init(void)
{
  app_iostream_usart_init();
  init_burtc();
  fragment_queue_init();
  graphics_init();
  app_button_pairing_init(button_event_handler);

  count = get_burtc_count();
  LOG_INFO("BURTC Count: %lu", (unsigned long)count);

  time = convert_count_to_seconds(count, 32768);
  LOG_INFO("Elapsed time (seconds): %lu", time);
}

// Application Process Action.
void app_process_action(void)
{
  sl_status_t sc;
  
  // Input buffer 
  static char buffer[BUFSIZE];

  // Notifiy
  if(notification)
  {
    sc = send_current_time_notification();
    if(sc == SL_STATUS_OK)
    {
      printf("send notification OK\r\n");
    }
    // sl_sleeptimer_delay_millisecond(DELAY_MS);
  }

  // Receive data and indication
  memset(buffer, 0, sizeof(buffer)-1);
  size_t len = read_line_from_iostream(sl_iostream_vcom_handle, (uint8_t *)buffer, BUFSIZE, 1000);
  if(len > 0)
  {
    // remove trailing CR/LF
    while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r'))
    {
      buffer[--len] = '\0';
    }
    LOG_INFO("\r\nReceived: %lu bytes: %s", len, (char *)buffer);

    sc = send_usart_packet_over_ble((uint8_t *)buffer, len);
    if(sc == SL_STATUS_OK)
    {
      LOG_INFO("send Indication OK");
    }
  }

  if (app_is_process_required()) {

  }
}

/**************************************************************************//**
 * Bluetooth stack event handler.
 * This overrides the default weak implementation.
 *
 * @param[in] evt Event coming from the Bluetooth stack.
 *****************************************************************************/
void sl_bt_on_event(sl_bt_msg_t *evt)
{
  sl_status_t sc;
  bd_addr address;
  uint8_t address_type;

  switch (SL_BT_MSG_ID(evt->header)) 
  {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:
      // Printf BLE version
      LOG_BOOT("Bluetooth stack booted: v%d.%d.%d+%08lxr\n",
                   evt->data.evt_system_boot.major,
                   evt->data.evt_system_boot.minor,
                   evt->data.evt_system_boot.patch,
                   evt->data.evt_system_boot.hash);
      
      // Extract unique ID from BT Address
      sc = sl_bt_gap_get_identity_address(&address, &address_type);
      app_assert_status(sc);
      LOG_BOOT("Bluetooth %d -> %s address: %02X:%02X:%02X:%02X:%02X:%02X",
                        (int)address_type,
                        address_type ? "static random" : "public device",
                        address.addr[5],
                        address.addr[4],
                        address.addr[3],
                        address.addr[2],
                        address.addr[1],
                        address.addr[0]);  

      // Configuration according to constants set at compile time
      // Configure security requirements and I/O capabilities of the system
      sc = sl_bt_sm_configure(MITM_PROTECTION, IO_CAPABILITY);
      app_assert_status(sc);
      LOG_BOOT("Passkey pairing mode");
      LOG_BOOT("Security level 4");
      LOG_BOOT("I/O DISPLAYYESNO");
      LOG_BOOT("Bonding with LE Secure mode, with authentication,...");

      // passkey = make_passkey_from_address(address);
      // LOG_BOOT("Passkey: %lu", passkey);
      // sc = sl_bt_sm_set_passkey(passkey);
      // app_assert_status(sc);
      // LOG_BOOT("Enter the fixed passkey for stack: %lu", passkey);

      sc = sl_bt_sm_set_bondable_mode(1);
      app_assert_status(sc);
      LOG_BOOT("Bondings allowed");

      sc = sl_bt_sm_delete_bondings();
      app_assert_status(sc);
      LOG_BOOT("Old bondings deleted");

      // Create an advertising set
      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      app_assert_status(sc);
      LOG_BOOT("Create an advertising set: %lu", (unsigned long)sc);

      // // Generate data for advertising
      // sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
      //                                            sl_bt_advertiser_general_discoverable);
      // app_assert_status(sc);
      // LOG_BOOT("Generate data for advertising: %lu", (unsigned long)sc);

      // Set advertising interval to 100ms.
      sc = sl_bt_advertiser_set_timing(
        advertising_set_handle,
        160, // min. adv. interval (milliseconds * 1.6)
        160, // max. adv. interval (milliseconds * 1.6)
        0,   // adv. duration
        0);  // max. num. adv. events
      app_assert_status(sc);
      LOG_BOOT("Set advertising interval to 100ms completed: %lu", (unsigned long)sc);

      sc = sl_bt_legacy_advertiser_set_data(advertising_set_handle,
                                            sl_bt_advertiser_advertising_data_packet,
                                            sizeof(adv_payload),
                                            adv_payload);

      // Start advertising and enable connections.
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                         sl_bt_legacy_advertiser_connectable);
      app_assert_status(sc);
      LOG_BOOT("Advertising %lu ", (unsigned long)sc);
      
      advertising =  true;
      if(advertising)
      {
        // Create timer for waking up the system periodically to print "."
        sl_sleeptimer_start_periodic_timer_ms(&timer_handle, 
                                              DELAY_MS, 
                                              timer_handler, 
                                              NULL, 0, 1);
      }
      break;

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      advertising = false;
      //connection_handle = evt->data.evt_gatt_server_characteristic_status.connection;
      connection_handle = evt->data.evt_connection_opened.connection;
      LOG_CONN("Connected to central device %02x\r\n", connection_handle);

      // // Enable encryption on an unencrypted device
      // sc = sl_bt_sm_increase_security(connection_handle);
      // app_assert_status(sc);
      // LOG_CONN("Enable encryption\r\n");
      break;

    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      // Error 0x1008 -> SL_STATUS_BT_CTRL_CONNECTION_TIMEOUT
      uint16_t reason = evt->data.evt_connection_closed.reason;
      LOG_CONN("Connection closed (handle=%d) reason=0x%02x (%d)",
               evt->data.evt_connection_closed.connection,
               reason, reason);

      sc = sl_bt_legacy_advertiser_set_data(advertising_set_handle,
                                            sl_bt_advertiser_advertising_data_packet,
                                            sizeof(adv_payload),
                                            adv_payload);
      app_assert_status(sc); 
      LOG_CONN("DISCONNECT: Generate data for advertising again");

      sc = sl_bt_sm_delete_bondings();
      app_assert_status(sc);
      LOG_CONN("All bonding deleted");

      // Restart advertising after client has disconnected.
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                         sl_bt_legacy_advertiser_connectable);
      app_assert_status(sc);
      LOG_CONN("Restart advertising");
      
      connection_handle = 0xFF;
      ind_state = INDICATION_DISABLE;
      advertising = true;
      state = IDLE;
      break;

    // -------------------------------
    // Triggered whenever the connection parameters are changed and at any
    // time a connection is established
    case sl_bt_evt_connection_parameters_id:
      switch(evt->data.evt_connection_parameters.security_mode)
      {
        case sl_bt_connection_mode1_level1:
          LOG_PAIRING("No Security\r\n");
          break;
        case sl_bt_connection_mode1_level2:
          LOG_PAIRING("[SEC-LEVEL] Encryption without unauthenticated (JustWorks)");
          break;
        case sl_bt_connection_mode1_level3:
          LOG_PAIRING("[SEC-LEVEL] Authenticated pairing with encryption (Legacy Pairing)");
          break;
        case sl_bt_connection_mode1_level4:
          LOG_PAIRING("[SEC-LEVEL] Authenticated LL Secure Connections with encryption");
          break;
        default:
          break;
      }
      break;

    // -------------------------------
    // Responder or Peripheral need to comfirm the bonding request
    case sl_bt_evt_sm_confirm_bonding_id:
      LOG_BONDING("Bonding confirmation request received");
      // Accept or reject the bonding request: 0-reject, 1 accept
      sc = sl_bt_sm_bonding_confirm(connection_handle, 1);
      app_assert_status(sc);
      LOG_BONDING("Bonding confirmed automatically (PassKey)");
      break;

    // -------------------------------
    // Identifier of the passkey_display event
    case sl_bt_evt_sm_passkey_display_id:
      // Display passkey
      LOG_PAIRING("evt_passkey_display Passkey: %lu", evt->data.evt_sm_passkey_display.passkey);
      passkey = evt->data.evt_sm_passkey_display.passkey;
      state = DISPLAY_PASSKEY;
      refresh_display();
      break;

    // -------------------------------
    // Identifier of the confirm_passkey event
    case sl_bt_evt_sm_confirm_passkey_id:
      LOG_PAIRING("Passkey confirmation event received");
      passkey = evt->data.evt_sm_confirm_passkey.passkey;  // CORRECT EVENT DATA

      // Enable button service for user input
      app_button_pairing_enable();

      state = PROMPT_YESNO;
      refresh_display();
      break;

    // -------------------------------
    // Triggered when the pairing or bonding procedure is successfully completed.
    case sl_bt_evt_sm_bonded_id:
      LOG_BONDING("Bonding process, bonding handle 0x%02x", evt->data.evt_sm_bonded.bonding);
      state = BOND_SUCCESS;
      refresh_display();
      break;

    // -------------------------------
    // Bonding failed, not affect the connection and exchange
    case sl_bt_evt_sm_bonding_failed_id:
      LOG_BONDING("Bonding failed, reason 0x%2X\r\n",
                evt->data.evt_sm_bonding_failed.reason);
      sc = sl_bt_connection_close(evt->data.evt_sm_bonding_failed.connection);
      LOG_BONDING("CLOSE connection");
      state = BOND_FAILURE;
      refresh_display();
      break;

    // -------------------------------
    case sl_bt_evt_system_external_signal_id:
      // Handle external signals
      if(evt->data.evt_system_external_signal.extsignals == PROMPT_CONFIRM_PASSKEY)
      {
        // Disable button service after user input
        // app_button_pairing_disable();

        LOG_PAIRING("User prompted to enter passkey: %lu", passkey);
        sc = sl_bt_sm_passkey_confirm(connection_handle, 1);
        if(sc == SL_STATUS_OK)
        {
          LOG_PAIRING("Passkey confirmed\r\n");
        }
      }
      break;

    // -------------------------------
    // This event indicates that the value of an attribute in the local GATT
    // database was changed by a remote GATT client.
    case sl_bt_evt_gatt_server_attribute_value_id:
      if(gattdb_usart_packet == evt->data.evt_gatt_server_attribute_value.attribute)
      {
        uint8_t data_recv[20];
        size_t data_recv_len;

        // Read characteristic value
        memset(data_recv, 0, sizeof(data_recv)-1);
        sc = sl_bt_gatt_server_read_attribute_value(gattdb_usart_packet,
                                                    0,
                                                    20,
                                                    &data_recv_len,
                                                    data_recv);
        (void)data_recv_len;   
        app_assert_status(sc);
        if (sc != SL_STATUS_OK) 
        {
          LOG_CONN("ERROR: Client wrote error");
          break;
        }

        data_recv[data_recv_len] = '\0'; 
        LOG_CONN("Written value by client: %s",data_recv);
        // LOG_INFO("Last byte: %c", data_recv[data_recv_len-1]);
      }
      break;

    // -------------------------------
    // This event occurs in two cases: 
    // when client send/dispatch GATT command to enable/disable the notification/indication for CCCD (1st bits or 2nd bits)
    // or when server obtains an ACK indication from client
    case sl_bt_evt_gatt_server_characteristic_status_id:
      // this field contains the characteristic handle which client changed its CCCD
      if(gattdb_current_time == evt->data.evt_gatt_server_characteristic_status.characteristic)
      {
        // A local Client Characteristic Configuration descriptor was changed in the gattdb_current_time characteristic.
        // sl_bt_gatt_notification = 0x1, /**< (0x1) Notification */
        // &and vs client_config_flags (0x01) to check if client enabled notification
        LOG_CONN("client_config_flags (gattdb_usart_packet) 0x%02x", 
                   evt->data.evt_gatt_server_characteristic_status.client_config_flags);
        if(evt->data.evt_gatt_server_characteristic_status.client_config_flags & sl_bt_gatt_notification)
        {
          LOG_CONN("Notification enabled");
          
          // Send notification of the current time
          sc = send_current_time_notification();
          app_assert_status(sc);
          printf("Sent current time\r\n");

          notification = true;
        }
        else
        {
          printf("Notification disabled\r\n");

          notification = false;
        }
      }

      // event: indication for usart packet characteristic
      if(gattdb_usart_packet == evt->data.evt_gatt_server_characteristic_status.characteristic)
      {
        // Checking the client_configs_flags is correct 0x02 sl_bt_gatt_indication, 0x02 & 0x02 = 0x02
        LOG_CONN("client_config_flags (gattdb_usart_packet) 0x%02x", 
                   evt->data.evt_gatt_server_characteristic_status.client_config_flags);
        if((evt->data.evt_gatt_server_characteristic_status.client_config_flags & sl_bt_gatt_indication) == 0x2
            && (ind_state == INDICATION_DISABLE))
        {
          ind_state = INDICATION_ENABLE;
          LOG_CONN("Indication enabled");

          sc = send_usart_packet_over_ble((uint8_t *)"WELCOME", 7);
          if(sc == SL_STATUS_OK)
          {
            LOG_CONN("Sent first indication");
          }
        }
        // verify the confirmation after every indication sent (sl_bt_gatt_server_confirmation (enum) and status_flags)
        else if((evt->data.evt_gatt_server_characteristic_status.status_flags == sl_bt_gatt_server_confirmation)
                  && (ind_state == INDICATION_ENABLE || ind_state == INDICATION_CONFIRM))
        {
          ind_state = INDICATION_CONFIRM;
          LOG_CONN("Client confirmed indication");
          fragment_queue_on_confirmation(connection_handle, gattdb_usart_packet);
        }
        else
        {
          ind_state = INDICATION_DISABLE;
          LOG_CONN("Indication disabled");
        }
      }

      // /**
      //  * @brief These values describe whether the characteristic client configuration
      //  * was changed or whether a characteristic confirmation was received.
      //  */
      // typedef enum
      // {
      //   sl_bt_gatt_server_client_config = 0x1, /**< (0x1) Characteristic client
      //                                               configuration has been changed. */
      //   sl_bt_gatt_server_confirmation  = 0x2  /**< (0x2) Characteristic confirmation
      //                                               has been received. */
      // } sl_bt_gatt_server_characteristic_status_flag_t;
      break;

    // -------------------------------
    // Default event handler.
    default:
      break;
  }
}

static void timer_handler(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void)data;
  (void)handle;

  if(advertising)
    printf(".");
}

/**
 * @brief Read a line from an iostream into a buffer with timeout.
 *
 * Reads available bytes from the given iostream handle until a newline
 * ('\n' or '\r') is encountered, the buffer is full (leaving room for a
 * null terminator), or the timeout (in milliseconds) expires. The function
 * polls the iostream and sleeps briefly when no data is available.
 *
 * @param handle   iostream handle to read from (e.g., `sl_iostream_vcom_handle`)
 * @param out_buf  Output buffer where data will be copied and null-terminated
 * @param max_len  Maximum number of bytes to write to out_buf (including null terminator)
 * @param timeout  Timeout in milliseconds to wait for a complete line
 * @return Number of bytes copied into out_buf (excluding null terminator)
 */
size_t read_line_from_iostream(sl_iostream_t *handle, uint8_t *out_buf, size_t max_len, uint32_t timeout)
{
  uint32_t waited = 0;
  size_t total = 0;
  uint8_t tempbuf[BUFSIZE];
  size_t bytes_read = 0;

  if(max_len == 0)
    return 0;
  
  // CR/LF = \r and \n
  // Clear output buffer to guarantee a valid C-string
    out_buf[0] = '\0';

    while (waited < timeout && total < (max_len - 1)) 
    {
        // read whatever data is available (up to tempbuf size)
        sl_status_t st = sl_iostream_read(handle, tempbuf, sizeof(tempbuf), &bytes_read);
        if (st == SL_STATUS_OK && bytes_read > 0) 
        {
            // Copy to output buffer
            LOG_INFO("bytes_read: %lu", bytes_read);
            size_t copy = bytes_read;
            if (total + copy > (max_len - 1)) 
            {
                // limit copy to remaining output buffer space
                copy = (max_len - 1) - total;
            }
            memcpy(out_buf + total, tempbuf, copy);
            total += copy;
            out_buf[total] = '\0';

            // return early if newline or CR present
            if (memchr(out_buf, '\n', total) != NULL || memchr(out_buf, '\r', total) != NULL) 
            {
                return total;
            }

            // Data received; reset timeout (optional) to wait for remaining bytes
            waited = 0;
            // Continue immediately to aggregate more data without a long delay
            continue;
        }

        // No data this iteration: sleep briefly to allow ISR/DMA to refill the buffer
        sl_sleeptimer_delay_millisecond(20);
        waited += 20;   // 10 is a poll interval
    }

    // On timeout, return any collected data; otherwise return 0
    return total;
} 

/**
 * @brief Assemble and send a Current Time notification to all connected clients.
 *
 * This helper reads the BURTC counter, converts it into elapsed seconds, and
 * constructs a 10-byte Current Time structure (year, month, day, hour, minute,
 * second, day_of_week, fractions256, adjust_reason) which is sent via
 * `sl_bt_gatt_server_notify_all()` on `gattdb_current_time`.
 *
 * @return SL_STATUS_OK on successful notification send, otherwise an error code
 */
static sl_status_t send_current_time_notification(void)
{
  sl_status_t sc;
  uint8_t current_time[10];     
  size_t len = 0;

  // Get current time from BURTC
  count = get_burtc_count();
  time = convert_count_to_seconds(count, 32768);
  LOG_CONN("Get current time from BURTC: %lu seconds", (unsigned long)time);

  // Timline (Milestone): 2025/11/07 03:40:10 (year=2025, month=11, day=7, hour=3, minute=40, second=10)
  const uint16_t base_year  = 2025;
  const uint8_t  base_month = 11;
  const uint8_t  base_day   = 7;
  const uint8_t  base_hour  = 3;
  const uint8_t  base_min   = 40;
  const uint8_t  base_sec   = 10;
  const uint8_t  base_dow   = 5;  // 5 is Friday (1->7) 

  // Get total seconds
  uint32_t total_seconds = (uint32_t)base_hour * 3600
                         + (uint32_t)base_min  * 60
                         + (uint32_t)base_sec
                         + time;

  // Calculate the days elapsed, hour, minutes and seconds
  uint32_t days_elapsed = total_seconds / 86400;
  uint32_t sec_of_day   = total_seconds % 86400;

  uint8_t hour   = sec_of_day / 3600;
  uint8_t minute = (sec_of_day % 3600) / 60;
  uint8_t second = (sec_of_day % 60);

  uint8_t day_of_week = (uint8_t)((base_dow + (days_elapsed % 7)));
  if (day_of_week > 7) 
  {
    day_of_week -= 7;
  }

  // Assuming the year, month, day is not changed
  uint16_t year  = base_year;
  uint8_t  month = base_month;
  uint8_t  day   = (uint8_t)(base_day + days_elapsed);
  // if day > 31, 30 or 28 -> handle the logic later

  // attach to data
  current_time[0] = (uint8_t)(year & 0xFF);
  current_time[1] = (uint8_t)((year >> 8) & 0xFF);
  current_time[2] = month;
  current_time[3] = day;
  current_time[4] = hour;
  current_time[5] = minute;
  current_time[6] = second;
  current_time[7] = day_of_week;
  current_time[8] = 0;      // Fractions256 = 0 if sub-second resolution is not supported
  current_time[9] = 0;      // Adjust Reason = 0 (no special reason)

  len = sizeof(current_time);

  // Send notification
  sc = sl_bt_gatt_server_notify_all(gattdb_current_time,
                                    len,
                                    current_time);
  if(sc == SL_STATUS_OK)
  {
    LOG_INFO("Notification sent: ");
    for(int i = 0; i < 10; i++)
    {
      printf("%02d : ", (int)current_time[i]);
    }
    printf("\r\n");
  }
  else
  {
    LOG_INFO("Notification sending failed");
  }

  return sc;
} 

/**
 * @brief Queue a USART payload for transmission over BLE using indications.
 *
 * This function validates that a connection is active and the payload length
 * is within the allowed range, then prepares fragment(s) for transmission by
 * calling `fragment_queue_prepare()` which handles fragmentation and flow
 * control. It returns the status code from the fragment queue helper or an
 * error if preconditions are not met.
 *
 * @param[in] payload Pointer to payload bytes
 * @param[in] payload_len Length of payload in bytes (must be >0 and <= DEFRAG_MAX_PAYLOAD)
 * @return SL_STATUS_OK if successfully queued, or an error status
 */
sl_status_t send_usart_packet_over_ble(uint8_t *payload, size_t payload_len)
{
  if (connection_handle == 0xFF) 
  {
    LOG_INFO("Connection_handle invalid");
    return SL_STATUS_BT_CTRL_UNKNOWN_CONNECTION_IDENTIFIER;
  }

  if (payload_len == 0 || payload_len > 200) 
  {
    LOG_INFO("ERROR: Invalid payload length %d (max 200)", (int)payload_len);
    return SL_STATUS_INVALID_PARAMETER; // 40bytes for 2 fragments
  }

  return fragment_queue_prepare(connection_handle, gattdb_usart_packet,
                                payload, payload_len);
} 

/*******************************************************************************
 ***************************   PASSKEY FUNCTIONS   *****************************
 ******************************************************************************/

// Make an passkey random from the device's address 
#if(IO_CAPABILITY != KEYBOARDONLY)
static uint32_t make_passkey_from_address(bd_addr address)
{
  static uint32_t passkey;
  // bd_addr has size = 6byte (uint8 addr[6])
  for(uint32_t i = 0; i < sizeof(bd_addr); i++)
  {
    passkey += (address.addr[i] << 8);
  }

  return passkey%1000000;   //6-digit
}
#endif

/*******************************************************************************
 ***************************   GRAPHIC FUNCTIONS   *****************************
 ******************************************************************************/

void graphics_init(void)
{
  EMSTATUS status;    // uint32_t

  status = sl_board_enable_display();
  if (status != SL_STATUS_OK) 
  {
    while (1) ;
  }

  /* Initialize the DMD module for the DISPLAY device driver. */
  status = DMD_init(0);
  if (DMD_OK != status) 
  {
    while (1) ;
  }

  LOG_INFO("[LCD] Enable display");
  status = GLIB_contextInit(&glibContext);
  if (GLIB_OK != status) 
  {
    while (1) ;
  }

  graphics_clear();

  glibContext.backgroundColor = Black;    
  glibContext.foregroundColor = White;    // foreground: nen truoc (thuong la chu)

  // Use Normal font
  GLIB_setFont(&glibContext, (GLIB_Font_t *)&GLIB_FontNormal8x8);

  graphics_AppendString(role_display_string);

  // Update Display always hase after drawing
  graphics_update();
}

void graphics_clear(void)
{
  GLIB_clear(&glibContext);

  // Reset the offset values to their defaults
  xOffset = X_BORDER;
  yOffset = Y_BORDER;
}

void graphics_update(void)
{
  DMD_updateDisplay();
}

void graphics_AppendString(char *str)
{
  GLIB_drawString(&glibContext, 
                  str, 
                  strlen(str), 
                  xOffset, 
                  yOffset, 
                  1);

  // glib_font_normal.h
  yOffset += glibContext.font.fontHeight + glibContext.font.lineSpacing;    
  //height of font(8pixels) and spacing of line (2pixels), avoid overiding the line above
}

void graphics_clear_PreviousString(void)
{
  yOffset -= (glibContext.font.fontHeight + glibContext.font.lineSpacing);   
  GLIB_Rectangle_t rect = {
    .xMin = xOffset,
    .yMin = yOffset,
    .xMax = xOffset + 128,
    .yMax = yOffset + glibContext.font.fontHeight + glibContext.font.lineSpacing
  };
  GLIB_setClippingRegion(&glibContext, &rect);
  GLIB_applyClippingRegion(&glibContext);
  GLIB_clearRegion(&glibContext);
}

void print_empty_line(uint8_t n_line)
{
  for(uint8_t i = 0; i < n_line; i++)
  {
    graphics_AppendString("");
  }
}

void refresh_display(void)
{
  switch (state)
  {
  case IDLE:
    break;
  case DISPLAY_PASSKEY:
    sprintf(passkey_display_string, "PASS: %lu", passkey);
    graphics_AppendString(passkey_display_string);
    break;
  case PROMPT_YESNO:
    sprintf(passkey_display_string, "PASS: %lu", passkey);
    graphics_AppendString(passkey_display_string);
    graphics_AppendString("   NO      YES");
    break;
  case BOND_SUCCESS:
    graphics_AppendString("BONDING SUCCESS");
    break;
  case BOND_FAILURE:
    graphics_AppendString("BONDING FAILURE");
    break;
  default:
    break;
  }

  graphics_update();
}

/*******************************************************************************
 ***************************   BUTTON HANDLER   ********************************
 ******************************************************************************/

void button_event_handler(const button_event_t *evt)
{
  // Handle button events
  if(evt->event_type == BUTTON_EVENT_PRESSED)
  {
    if(state == PROMPT_YESNO)
    {
      LOG_PAIRING("User response received: %s", 
                    (evt->button_id == BUTTON_ID_0) ? "YES" : "NO");

      // No acception for calling API ble Gecko in ISR context
      state = PROMPT_CONFIRM_PASSKEY;
      sl_bt_external_signal(PROMPT_CONFIRM_PASSKEY);        
    }
  }
}
