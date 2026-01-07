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
#include "sl_bt_api.h"
#include "sl_main_init.h"
#include "app_assert.h"
#include "app.h"
#include "sl_component_catalog.h"
#include "sl_bluetooth.h"
#include "sl_status.h"
#include "sl_sleeptimer.h"

#include "app_iostream_usart.h"
#include "ble_defragment_rxdata.h"
#include "app_button_pairing_complete.h"

#include "sl_board_control.h"
#include "dmd.h"
#include "glib.h"

// default: define SL_BT_CONFIG_MAX_CONNECTIONS (4)
#if SL_BT_CONFIG_MAX_CONNECTIONS < 1
  #error At least 1 connection has to be enabled!
#endif

// Connection parameters
#define CONN_INTERVAL_MIN             80   // 100ms
#define CONN_INTERVAL_MAX             100  // 100*10ms
#define CONN_RESPONDER_LATENCY        0    // no latency
#define CONN_TIMEOUT                  500  // 500*10ms
#define CONN_MIN_CE_LENGTH            0
#define CONN_MAX_CE_LENGTH            0xffff

#define CONNECTION_HANDLE_INVALID     ((uint8_t)0xFFu)
#define SERVICE_HANDLE_INVALID        ((uint32_t)0xFFFFFFFFFFFFFu)
#define CHARACTERISTIC_HANDLE_INVALID ((uint16_t)0xFFFFu)
#define TABLE_INDEX_INVALID           ((uint8_t)0xFFu)
#define TX_POWER_INVALID              ((uint8_t)0x7C)
#define TX_POWER_CONTROL_ACTIVE       ((uint8_t)0x00)
#define TX_POWER_CONTROL_INACTIVE     ((uint8_t)0x01)
#define PRINT_TX_POWER_DEFAULT        (false)

#define TABLE_INDEX_INVALID           ((uint8_t)0xFFu)

#define DISPLAYONLY       0
#define DISPLAYYESNO      1
#define KEYBOARDONLY      2
#define NOINPUTNOOUTPUT   3
#define KEYBOARDDISPLAY   4

// Numeric Comparison
#define MITM_PROTECTION      (0x01)
#define IO_CAPABILITY        (DISPLAYYESNO)     //KEYBOARDDISPLAY

#ifndef IO_CAPABILITY 
#error "Must define which IO capability device supports\r\n"
#endif

// Relate to display 
#define X_BORDER    0
#define Y_BORDER    0
#define SCREEN_REFRESH_PERIOD   (32768/4)   // Not use

typedef enum
{
  scanning,
  opening,
  pairing,
  discover_services,
  discover_characteristics,
  enable_indication,
  running,
  handle_rxdata
} conn_state_t;

typedef enum
{
  IDLE,
  DISPLAY_PASSKEY,
  PROMPT_YESNO,
  PROMPT_CONFIRM_PASSKEY,
  BOND_SUCCESS,
  BOND_FAILURE
} pair_state_t;

typedef struct {
  uint8_t  connection_handle;
  int8_t   rssi;
  bool     power_control_active;
  int8_t   tx_power;
  int8_t   remote_tx_power;
  uint8_t  server_address[6];
  uint32_t usart_service_handle;
  uint16_t usartpacket_characteristic_handle;
} conn_properties_t;

// Array for holding properties of multiple (parallel) connections
conn_properties_t conn_properties[SL_BT_CONFIG_MAX_CONNECTIONS];

// Counter of active connections
static uint8_t active_connections_num;

// This variable holds the connection handle of the current connection
// serving for evt confirm_passkey
static uint8_t temp_connec_handle;

// State of connection under establishment
conn_state_t conn_state;
conn_state_t indi_state;
static volatile pair_state_t state = IDLE;

// [DISPLAY] Default strings and context used to show role and passkey on the LCD display
static char role_display_string[] = "   INITIATOR   ";
static char passkey_display_string[] = "00000000000000";
static uint32_t xOffset, yOffset;
static GLIB_Context_t glibContext;
static volatile uint32_t passkey = 0;

// My custom service UUID in gattdb (server)
// I need AD type 0x07 -> complete list of custom services (128bits)
// 8935c600-3a0e-4388-92ed-8f6de23f3f5a -> convert Little endian: 5a3f3fe26d8fed9288430e3a00c63589
static const uint8_t current_time_service[2] = { 0x05, 0x18 };
static const uint8_t name_service[2] = { 0x00, 0x18 };
static const uint8_t name_characteristic[2] = { 0x00, 0x2A };
static const uint8_t usart_service[16] = { 0x40, 0x30, 0x57, 0x13, 0x72, 0xd9, 0x62, 0x83, 
                                           0xdf, 0x4c, 0xb8, 0x80, 0xd9, 0x81, 0x7d, 0x46 };
static const uint8_t usart_char[16] = { 0xfa, 0x3d, 0x74, 0x7c, 0x09, 0xd3, 0xdf, 0xb1, 
                                        0x07, 0x41, 0xd4, 0xa2, 0xa5, 0x79, 0xba, 0x17 };

// Init properties
static void init_properties(void);

// Show bluetooth address
static bd_addr *read_and_cache_bluetooth_address(uint8_t *address_type_out);
static void printf_bluetooth_address(void);

// Find service
static sl_status_t find_service_in_advertisement(uint8_t *data, uint8_t len);

// Add connection with server
static uint8_t find_index_by_connection_handle(uint8_t connection);
static void add_connection(uint8_t connection, uint8_t *address);
static void remove_connection(uint8_t connection);

// SERCURITY, PASSKEY
void graphics_init(void);
void graphics_clear(void);
void graphics_update(void);
void graphics_AppendString(char *str);
void graphics_clear_PreviousString(void);
void print_empty_line(uint8_t n_line);
// Depends on pair_state_t to display any string (passkey, bonding state, ...)
void refresh_display(void);   // will be called in timer callback or loopback

// Callbacks function for button events
void button_event_handler(const button_event_t *evt);

// Application Init.
void app_init(void)
{
  app_iostream_usart_init();
  init_properties();
  defrag_init();
  graphics_init();
  app_button_pairing_init(button_event_handler);
}

// Application Process Action.
void app_process_action(void)
{
  if(indi_state == handle_rxdata)
  {
    defrag_enum_t rx_data_state = defrag_process_fragment();
    if(rx_data_state == DEFRAG_COMPLETE)
    {
      uint8_t *payload;
      uint16_t payload_len;
      bool checksum_ok;
      
      if(defrag_get_payload(&payload, &payload_len, &checksum_ok))
      {
        if (checksum_ok)
        {
          LOG_INFO("->Payload Ready:");
          LOG_INFO("->Length: %d bytes", (int)payload_len);
          LOG_INFO("->Data: \"%.*s\" ", (int)payload_len, payload);
          
        }
        else
        {
          LOG_INFO("Checksum error");
        }
      }

      // Reset for next transmission
      defrag_reset();
    }
    else if(rx_data_state == DEFRAG_ERROR)
    {
      LOG_INFO("[ERROR] Defragmentation error");
      defrag_reset();
    }

    indi_state = running;
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
  uint8_t addr_value[6];
  uint8_t table_index;

  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:
      state = IDLE;
      // Show the device information
      LOG_BOOT("Bluetooth stack booted: v%d.%d.%d+%08lx\r\n",    //version 10.1.1 (major.minor.patch)
              evt->data.evt_system_boot.major,
              evt->data.evt_system_boot.minor,
              evt->data.evt_system_boot.patch,
              evt->data.evt_system_boot.hash); 

      printf_bluetooth_address();
      
      // Configuration according to constants set at compile time
      // Configure security requirements and I/O capabilities of the system
      sc = sl_bt_sm_configure(MITM_PROTECTION, IO_CAPABILITY);
      app_assert_status(sc);
      LOG_BOOT("Passkey pairing mode");
      LOG_BOOT("Security level 4");
      LOG_BOOT("I/O DISPLAYYESNO");
      LOG_BOOT("Bonding with LE Secure mode, with authentication,...");

      passkey = make_passkey_from_address(address);
      LOG_BOOT("Passkey: %lu", passkey);
      sc = sl_bt_sm_set_passkey(passkey);
      app_assert_status(sc);
      LOG_BOOT("Enter the fixed passkey for stack: %lu", passkey);

      sc = sl_bt_sm_set_bondable_mode(1);
      app_assert_status(sc);
      LOG_BOOT("Bondings allowed");

      sc = sl_bt_sm_delete_bondings();
      app_assert_status(sc);
      LOG_BOOT("Old bondings deleted");

      // Set the default connection parameters for subsequent connections
      sc = sl_bt_connection_set_default_parameters(CONN_INTERVAL_MIN,
                                                   CONN_INTERVAL_MAX,
                                                   CONN_RESPONDER_LATENCY,
                                                   CONN_TIMEOUT,
                                                   CONN_MIN_CE_LENGTH,
                                                   CONN_MAX_CE_LENGTH);   
      app_assert_status(sc);

      // Start scanning
      sc = sl_bt_scanner_start(sl_bt_scanner_scan_phy_1m,
                               sl_bt_scanner_discover_generic);
      app_assert_status(sc);
      app_assert_status_f(sc, "Failed to start discovery #1\r\n");
      LOG_SCANN("Started scanning %02lx", sc);

      conn_state = scanning;
      break;

    // -------------------------------
    // This event indicates that central receive a adv_pack or scan_reponse_pack
    // old sdk: sl_bt_evt_scanner_scan_report
    case sl_bt_evt_scanner_legacy_advertisement_report_id:
      // Parse the advertisment packets
      // SL_BT_SCANNER_EVENT_FLAG_CONNECTABLE   0x1 -> connectable: peripherals accept connection from centrals
      // SL_BT_SCANNER_EVENT_FLAG_SCANNABLE     0x2 -> scannable: peripherals enable "active scanning" mode
      // SL_BT_SCANNER_EVENT_FLAG_DIRECTED      0x4 -> packet contains infor of a particular device
      // SL_BT_SCANNER_EVENT_FLAG_SCAN_RESPONSE 0x8 -> scan response packet
      if(evt->data.evt_scanner_legacy_advertisement_report.event_flags 
         == (SL_BT_SCANNER_EVENT_FLAG_CONNECTABLE | SL_BT_SCANNER_EVENT_FLAG_SCANNABLE))
      {
        // find service usart_ser advertisment packet
        if(find_service_in_advertisement(&(evt->data.evt_scanner_legacy_advertisement_report.data.data[0]),
                                         evt->data.evt_scanner_legacy_advertisement_report.data.len) == SL_STATUS_OK)
        {
          LOG_SCANN("Discover/find my service in AD structure");

          // Then stop scanning for a while
          sc = sl_bt_scanner_stop();
          app_assert_status(sc);
          LOG_SCANN("Stopped scanning after finding my service");

          // And connect to that device, guarantee the number of connections < Max
          if (active_connections_num < SL_BT_CONFIG_MAX_CONNECTIONS) {
            LOG_CONN("Connecting to the central device, active_connection_num %d", (int)active_connections_num);
            sc = sl_bt_connection_open(evt->data.evt_scanner_legacy_advertisement_report.address,
                                       evt->data.evt_scanner_legacy_advertisement_report.address_type,
                                       sl_bt_gap_phy_1m,
                                       NULL);
            app_assert_status(sc);
            LOG_CONN("Connection request sent");

            conn_state = opening;
          }
        }
      }
      break;

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      LOG_CONN("Connected with that device");
      LOG_CONN("Pairing process before discovering services");

      // [Note]: Initially, I discovered services right after connecting, but when adding sercurity features
      // the pairing/bonding phase, we should perform pairing first before discovering. Therefore, this discovery
      // phase will be moved to "sl_bt_evt_sm_bonded_id" event after bonding is completed successfully.
      // And the connection to the table conn_properties will be added in "sl_bt_evt_sm_bonded_id" event.
      sc = sl_bt_sm_increase_security(evt->data.evt_connection_opened.connection);
      app_assert_status(sc);
      if(sc == SL_STATUS_OK)
      {
        LOG_CONN("sl_bt_sm_increase_security returned 0x%02lx", sc);
      }
      LOG_CONN("[SECURITY] Enable encryption");

      // Reserve the address of connected device
      memcpy(addr_value, evt->data.evt_connection_opened.address.addr, 6);
      //  Add connection to the connection_properties array
      add_connection(evt->data.evt_connection_opened.connection, addr_value);
      LOG_CONN("Reserved the addr of server device: ");
      for(int i = 5; i >= 0; i--)
        printf("%02X : ", addr_value[i]);
      printf("\r\n");

      temp_connec_handle = evt->data.evt_connection_opened.connection;

      conn_state = pairing;
      break;

    // -------------------------------
    // This event is generated when a new service is discovered.
    // Reserve service handle
    case sl_bt_evt_gatt_service_id:
      table_index = find_index_by_connection_handle(evt->data.evt_gatt_service.connection);
      if (table_index != TABLE_INDEX_INVALID) 
      {
        // Save service handle for future reference
        conn_properties[table_index].usart_service_handle = evt->data.evt_gatt_service.service;
        LOG_DISC("Service handle was received: %d", (int)evt->data.evt_gatt_service.service);
      }
      break;

    // -------------------------------
    // This event is generated when a new characteristic is discovered.
    // The characteristic discovery will be perform in sl_bt_evt_gatt_procedure_completed_id event when service discovery is completed
    // Reserve characteristic handle
    case sl_bt_evt_gatt_characteristic_id:
      table_index = find_index_by_connection_handle(evt->data.evt_gatt_characteristic.connection);
      if (table_index != TABLE_INDEX_INVALID) 
      {
        // Save service handle for future reference
        conn_properties[table_index].usartpacket_characteristic_handle = evt->data.evt_gatt_characteristic.characteristic;
        LOG_DISC(">Characteristic handle was received: %d", (int)evt->data.evt_gatt_characteristic.characteristic);
      }
      break;

    // -------------------------------
    // This event is generated for various procedure completions, e.g. when a
    // write procedure is completed, or service discovery is completed
    case sl_bt_evt_gatt_procedure_completed_id:
      // Indicates that the current GATT procedure was completed successfully
      // or that it failed with an error
      table_index = find_index_by_connection_handle(evt->data.evt_gatt_procedure_completed.connection);
      if (table_index == TABLE_INDEX_INVALID) 
      { 
        break;
      }

      // The service discovery was completed successfully. Start discovering characteristic
      if(conn_state == discover_services && conn_properties[table_index].usart_service_handle != SERVICE_HANDLE_INVALID)
      {
        sc = sl_bt_gatt_discover_characteristics_by_uuid(evt->data.evt_gatt_procedure_completed.connection,   // connection
                                                        conn_properties[table_index].usart_service_handle,   // service
                                                        sizeof(usart_char),                                  // uuid_len
                                                        (const uint8_t*)usart_char);                         // uuid    
        app_assert_status(sc);
        LOG_DISC("Discovering charateristic and success");
        conn_state = discover_characteristics;
        break;
      }

      // If characteristic discovery was completed successfully or failed with an error
      // It will be join sl_bt_evt_gatt_characteristic_id event before returning this event
      // -> enable indications
      if(conn_state == discover_characteristics && conn_properties[table_index].usartpacket_characteristic_handle != CHARACTERISTIC_HANDLE_INVALID)
      {
        LOG_DISC("Characteristic discovery was completed");
        // stop discovering
        sl_bt_scanner_stop();
        sc = sl_bt_gatt_set_characteristic_notification(evt->data.evt_gatt_procedure_completed.connection,
                                                        conn_properties[table_index].usartpacket_characteristic_handle,
                                                        sl_bt_gatt_indication);
        app_assert_status(sc);
        LOG_DISC("Set indication configuration flag into this characteristic");
        conn_state = enable_indication;
        break;                                                        
      }

      // If enabling indication finish, connect to other devices
      if(conn_state == enable_indication)
      {
        if(active_connections_num < SL_BT_CONFIG_MAX_CONNECTIONS)
        {
          LOG_CONN("Active connection number %d\r\nStart scanning other devices", active_connections_num);

          sc = sl_bt_scanner_start(sl_bt_scanner_scan_phy_1m,
                                  sl_bt_scanner_discover_generic);
          app_assert_status_f(sc, ">Failed to start discovery #2" APP_LOG_NL);
          conn_state = scanning;
        }
        else
        {
          conn_state = running;
        }
        break;
      }
      break;

    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      sc = sl_bt_sm_delete_bondings();
      app_assert_status(sc);
      LOG_BONDING("[SECURITY] All bonding deleted\r\n");

      // remove connection from active connections
      remove_connection(evt->data.evt_connection_closed.connection);
      LOG_CONN(">Connection is CLOSE. Active connections: %d\r\n", active_connections_num);
      if (conn_state != scanning) 
      {
        // start scanning again to find new devices
        sc = sl_bt_scanner_start(sl_bt_scanner_scan_phy_1m,
                                 sl_bt_scanner_discover_generic);
        app_assert_status_f(sc, ">Failed to start discovery #3" APP_LOG_NL);
        LOG_SCANN(">RESTART scanning\r\n");
        conn_state = scanning;
      }
      break;

    // -------------------------------
    // This event is generated when a characteristic value was received e.g. an gatt server sends indication
    // or notification after enabling (clients) with api: sl_bt_gatt_set_characteristic_notification()
    case sl_bt_evt_gatt_characteristic_value_id:
      table_index = find_index_by_connection_handle(evt->data.evt_gatt_characteristic_value.connection);
      if(table_index == TABLE_INDEX_INVALID)
      {
        break;
      }

      if(evt->data.evt_gatt_characteristic_value.value.len > 0)
      {
        uint8_t *data = evt->data.evt_gatt_characteristic_value.value.data;
        uint8_t len = evt->data.evt_gatt_characteristic_value.value.len;
        
        // Print and process Input data
        if(defrag_push_data(data, len))
        {
          LOG_CONN("DONE PUSH data");
          indi_state = handle_rxdata;
        }
      }

      sc = sl_bt_gatt_send_characteristic_confirmation(evt->data.evt_gatt_characteristic_value.connection);
      app_assert_status(sc);
      LOG_CONN"Send an indication confirmation");
      break;
    
    // -------------------------------
    // Triggered whenever the connection parameters are changed and at any
    // time a connection is established
    case sl_bt_evt_connection_parameters_id:
      switch(evt->data.evt_connection_parameters.security_mode)
      {
        case sl_bt_connection_mode1_level1:
          LOG_PAIRING("[SEC-LEVEL] No Security");
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
      LOG_BONDING("Bonding confirmation request received\r\n");
      // Accept or reject the bonding request: 0-reject, 1 accept
      sc = sl_bt_sm_bonding_confirm(evt->data.evt_sm_confirm_bonding.connection, 1);
      app_assert_status(sc);
      LOG_BONDING("Bonding confirmed automatically (PassKey)\r\n");
      break;

    // -------------------------------
    // Identifier of the passkey_display event
    case sl_bt_evt_sm_passkey_display_id:
      // Display passkey
      LOG_PAIRING("evt_passkey_display Passkey: %lu\r\n", evt->data.evt_sm_passkey_display.passkey);
      passkey = evt->data.evt_sm_passkey_display.passkey;
      state = DISPLAY_PASSKEY;
      refresh_display();
      break;

    // -------------------------------
    // Identifier of the confirm_passkey event
    case sl_bt_evt_sm_confirm_passkey_id:
      LOG_PAIRING("Passkey confirmation event received");
      passkey = evt->data.evt_sm_confirm_passkey.passkey;  //CORRECT EVENT DATA

      // Enable button service for user input
      app_button_pairing_enable();

      state = PROMPT_YESNO;
      refresh_display();
      break;

    // -------------------------------
    // Triggered when the pairing or bonding procedure is successfully completed.
    case sl_bt_evt_sm_bonded_id:
      LOG_BONDING("Bond success, bonding handle 0x%02x", evt->data.evt_sm_bonded.bonding);

      //  * Discover primary services with the specified UUID in a remote GATT database.
      //  * This command generates unique gatt_service event for every discovered primary
      //  * service. Received @ref sl_bt_evt_gatt_procedure_completed event indicates
      //  * that this GATT procedure was successfully completed or failed with an error.
      sc = sl_bt_gatt_discover_primary_services_by_uuid(evt->data.evt_sm_bonded.connection,
                                                        sizeof(usart_service),
                                                        (const uint8_t *)usart_service);
      if (sc == SL_STATUS_INVALID_HANDLE) 
      {       
        // Not found service with given UUID      
        // Failed to open connection, restart scanning  
        LOG_DISC(">Primary service discovery failed with invalid handle, dropping client");
        sc = sl_bt_connection_close(evt->data.evt_sm_bonded.connection);
        LOG_DISC("Close connection");

        sc = sl_bt_scanner_start(sl_bt_gap_phy_1m, sl_bt_scanner_discover_generic);
        app_assert_status(sc);
        conn_state = scanning;
        break;
      } 
      else 
      {
        app_assert_status(sc);
        LOG_DISC("-> Confirm the existence of my service in remote GATT database");
      } 

      state = BOND_SUCCESS;
      conn_state = discover_services;
      refresh_display();
      break;

    // Bonding failed, not affect the connection and exchange
    case sl_bt_evt_sm_bonding_failed_id:
      LOG_BONDING("Bonding failed, reason 0x%2X",
                evt->data.evt_sm_bonding_failed.reason);
      sc = sl_bt_connection_close(evt->data.evt_sm_bonding_failed.connection);
      LOG_BONDING("CLOSE connection");

      state = BOND_FAILURE;
      refresh_display();
      break;

    case sl_bt_evt_system_external_signal_id:
      // Handle external signals
      if(evt->data.evt_system_external_signal.extsignals == PROMPT_CONFIRM_PASSKEY)
      {
        // Disable button service after user input
        // app_button_pairing_disable();

        LOG_PAIRING("User prompted to enter passkey: %lu", passkey);
        sc = sl_bt_sm_passkey_confirm(temp_connec_handle, 1);
        if(sc == SL_STATUS_OK)
        {
          LOG_PAIRING("Passkey confirmed");
        }
      }
      break;

    // -------------------------------
    // Default event handler.
    default:
      break;
  }
}

/**
 * @brief Initialize connection properties table and counters.
 *
 * This function resets the internal `conn_properties` table to a known
 * default state and sets the active connection count to zero. Each entry is
 * initialized so callers can reliably check for `CONNECTION_HANDLE_INVALID`
 * to find free slots. Call once at startup and after major state resets.
 */
static void init_properties(void)
{
  uint8_t i;
  active_connections_num = 0;

  for (i = 0; i < SL_BT_CONFIG_MAX_CONNECTIONS; i++) {
    conn_properties[i].connection_handle = CONNECTION_HANDLE_INVALID;
    conn_properties[i].usart_service_handle = SERVICE_HANDLE_INVALID;
    conn_properties[i].usartpacket_characteristic_handle = CHARACTERISTIC_HANDLE_INVALID;
    conn_properties[i].rssi = SL_BT_CONNECTION_RSSI_UNAVAILABLE;    // in sl_bt_api.h file          
    conn_properties[i].power_control_active = TX_POWER_CONTROL_INACTIVE;
    conn_properties[i].tx_power = TX_POWER_INVALID;
    conn_properties[i].remote_tx_power = TX_POWER_INVALID;
  }
}

/**
 * @brief Function to Read and Cache Bluetooth Address.
 * 
 * @param[out] address_type_out 
 *    A pointer to the outgoing address_type. This pointer can be NULL.
 * @return Pointer to the cached Bluetooth Address
*/
static bd_addr *read_and_cache_bluetooth_address(uint8_t *address_type_out)
{
  static bd_addr address;         // static -> local variable is allocated memory when starting main -> no unbehavior
  static uint8_t address_type;    // the value of static local vari is not declared when calling function again
  static bool cached = false;

  if (!cached) 
  {
    sl_status_t sc = sl_bt_gap_get_identity_address(&address, &address_type);
    app_assert_status(sc);
    cached = true;
  }

  if (address_type_out) 
  {
    // <b>sl_bt_gap_public_address (0x0):</b> Public device address
    // <b>sl_bt_gap_static_address (0x1):</b> Static device address
    *address_type_out = address_type;
    LOG_INFO("Address type: %d", (int)(*address_type_out));
  }

  return &address;
}

/**
 * @brief Print the cached Bluetooth address to the console/log.
 *
 * This helper obtains/receives the local device address via `read_and_cache_bluetooth_address`
 * and prints it using `LOG_INFO`. The address type (public vs static random)
 * is also printed for diagnostic purposes.
 */
void printf_bluetooth_address(void)
{
  uint8_t address_type;
  bd_addr *address = read_and_cache_bluetooth_address(&address_type);

  LOG_INFO("Bluetooth %s address: %02X:%02X:%02X:%02X:%02X:%02X",
               address_type ? "static random" : "public device",
               address->addr[5],
               address->addr[4],
               address->addr[3],
               address->addr[2],
               address->addr[1],
               address->addr[0]);
}

/**
 * @brief Scan an advertisement packet's AD structures for known service UUIDs.
 *
 * @param[in] data Pointer to the advertisement payload (AD structures)
 * @param[in] len  Length of the advertisement payload in bytes
 * @return `SL_STATUS_OK` if a matching UUID is found, `SL_STATUS_FAIL` otherwise
 */
static sl_status_t find_service_in_advertisement(uint8_t *data, uint8_t len)
{
  // Information of advertised data (AD structure)
  uint8_t ad_field_length;  // length 1Byte
  uint8_t ad_field_type;    // type 1Byte
  uint8_t i = 0;
  // Parse advertisement packet
  // LOG_INFO("Len adv packet %d", (int)len);
  while (i < len) 
  {
    ad_field_length = data[i];
    ad_field_type = data[i + 1];

    if(ad_field_type == 0x06 || ad_field_type == 0x07)    //ad_field_type == 0x02 || ad_field_type == 0x03 ||
    {
      if(memcmp(&data[i+2], current_time_service, 2) == 0)
      {
        return SL_STATUS_OK;
      }
      else if (memcmp(&data[i+2], usart_service, 16) == 0)
      {
        LOG_SCANN("Found my service's UUID");
        return SL_STATUS_OK;
      }
    }
    // advance to the next AD struct
    i = i + ad_field_length + 1;
    // sl_sleeptimer_delay_millisecond(10);
  }

  return SL_STATUS_FAIL;
}

/**
 * @brief Find the table index for a given connection handle.
 *
 * Searches the active portion of the `conn_properties` table for an entry
 * whose `connection_handle` matches the input. If found, returns the index
 * (0..active_connections_num-1). If no matching entry exists, returns
 * `TABLE_INDEX_INVALID`.
 *
 * @param connection Connection handle to look up
 * @return Index in `conn_properties` if found, otherwise `TABLE_INDEX_INVALID`
 */
static uint8_t find_index_by_connection_handle(uint8_t connection)
{
  // i will be adapt active_connections_num
  for (uint8_t i = 0; i < active_connections_num; i++) 
  {
    if (conn_properties[i].connection_handle == connection) 
    {
      return i;
    }
  }
  return TABLE_INDEX_INVALID;
}

/**
 * @brief Add a new active connection to the `conn_properties` table.
 *
 * Note: The caller must ensure `active_connections_num < SL_BT_CONFIG_MAX_CONNECTIONS`
 * before calling this function. The implementation does not perform bounds
 * checks and will overwrite memory if the caller violates this contract.
 *
 * @param[in] connection The connection handle assigned by the stack
 * @param[in] address    Pointer to a 6-byte Bluetooth address (LSB-first ordering)
 */
static void add_connection(uint8_t connection, uint8_t *address)
{
  conn_properties[active_connections_num].connection_handle = connection;
  memcpy(conn_properties[active_connections_num].server_address, address, 6);
  active_connections_num++;
}

/**
 * @brief Remove an active connection and compact the table.
 *
 * @param[in] connection Connection handle to remove
 */
static void remove_connection(uint8_t connection)
{
  uint8_t i;
  uint8_t table_index = find_index_by_connection_handle(connection);

  if (active_connections_num > 0) 
  {
    active_connections_num--;
  }
  // Shift entries after the removed connection toward 0 index
  for (i = table_index; i < active_connections_num; i++) 
  {
    conn_properties[i] = conn_properties[i + 1];
  }
  // Clear the slots we've just removed so no junk values appear
  for (i = active_connections_num; i < SL_BT_CONFIG_MAX_CONNECTIONS; i++) 
  {
    conn_properties[i].connection_handle = CONNECTION_HANDLE_INVALID;
    conn_properties[i].usart_service_handle = SERVICE_HANDLE_INVALID;
    conn_properties[i].usartpacket_characteristic_handle = CHARACTERISTIC_HANDLE_INVALID;
    conn_properties[i].rssi = SL_BT_CONNECTION_RSSI_UNAVAILABLE;
    conn_properties[i].power_control_active = TX_POWER_CONTROL_INACTIVE;
    conn_properties[i].tx_power = TX_POWER_INVALID;
    conn_properties[i].remote_tx_power = TX_POWER_INVALID;
  }
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
 *******************************************************************************/

void graphics_init(void)
{
  EMSTATUS status;    

  status = sl_board_enable_display();
  if (status != SL_STATUS_OK) 
  {
    while (1) ;
  }

  // Initialize the DMD module for the DISPLAY device driver
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
    graphics_clear_PreviousString();
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
 *******************************************************************************/

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