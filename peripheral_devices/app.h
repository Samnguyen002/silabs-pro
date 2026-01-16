/***************************************************************************//**
 * @file
 * @brief Application interface.
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef APP_H
#define APP_H

#include <stdbool.h>
#include "sl_bt_api.h"

/// GATT
#define UUID_16_LEN                   2
#define UUID_128_LEN                  16
#define HANDLE_NOT_INITIALIZED        0

#define GAP_INCOMPLETE_16B_UUID       0x02 // Incomplete List of 16-bit Service Class UUIDs
#define GAP_COMPLETE_16B_UUID         0x03 // Complete List of 16-bit Service Class UUIDs
#define GAP_INCOMPLETE_128B_UUID      0x06 // Incomplete List of 128-bit Service Class UUIDs
#define GAP_COMPLETE_128B_UUID        0x07 // Complete List of 128-bit Service Class UUIDs

/// Data lengths
#define CHAIN_LINK_DATA_LEN           192  // Length of an NVM3 chunk
#define CHAIN_LINK_DATA_NUM           4    // Number of how many chunks needed for a certificate

#define CERT_IND_CHUNK_LEN            100
#define EC_PUB_KEY_LEN                65
#define PUB_KEY_OFFSET                26

#define PASSKEY_LEN                   4    // PassKey is 6-digit number (stored as uint32_t)
#define SIGNATURE_LEN                 64   // ECDSA signature length
#define SIGNATURE_DATA_LEN            (PASSKEY_LEN + SIGNATURE_LEN)

// -----------------------------------------------------------------------------
// Type definitions

/// Connection properties
typedef struct {
  uint8_t  connection_handle;
  bd_addr  address;
  int8_t   rssi;
  bool     power_control_active;
  int8_t   tx_power;
  int8_t   remote_tx_power;
  uint8_t  server_address[6];
  uint32_t usart_service_handle;
  uint16_t usartpacket_characteristic_handle;
  uint32_t cbap_service_handle;
} conn_properties_t;

/// Characteristic properties
typedef struct characteristic_128_ref_s {
  uint16_t handle;
  uint8_t uuid[UUID_128_LEN];
} characteristic_128_ref_t;

/// GATT UUIDs
#define CBAP_SERVICE_UUID                         \
  0x4d, 0xf0, 0x77, 0x8a, 0x9e, 0x9e, 0x9b, 0xa6, \
  0x3f, 0x43, 0x0d, 0xcf, 0xe1, 0x08, 0x39, 0xf4
#define CENTRAL_CERT_CHAR_UUID                    \
  0xdd, 0xb9, 0x0d, 0xf8, 0x66, 0x08, 0x51, 0x99, \
  0xd6, 0x4a, 0x75, 0xa8, 0xae, 0x34, 0x58, 0xf2
#define PERIPHERAL_CERT_CHAR_UUID                 \
  0xa3, 0xcf, 0x2f, 0x16, 0xed, 0xb8, 0x78, 0x80, \
  0xd4, 0x41, 0xa8, 0x3f, 0x45, 0x2f, 0x39, 0xe0
#define INITIATOR_PASSKEY_CHAR_UUID                     \
  0x65, 0xdc, 0x9d, 0x54, 0xc4, 0x93, 0x30, 0xb6, \
  0xe0, 0x4c, 0xf6, 0xf7, 0x72, 0x41, 0x4a, 0xdd

typedef enum {
  CHAR_CENTRAL_CERT,
  CHAR_PERIPHERAL_CERT,
  CHAR_INITIATOR_PASSKEY,
  CHAR_NUM                // Position corresponding to the number of characteristics
} characteristics_t;

/// Central device states
typedef enum {
  CENTRAL_SCANNING,
  CENTRAL_OPENNING,
  CENTRAL_DISCOVER_SERVICES,
  CENTRAL_DISCOVER_CHARACTERISTICS,
  CENTRAL_GET_PERIPHERAL_CERT,
  CENTRAL_SEND_CENTRAL_CERT,
  CENTRAL_GET_PASSKEY,
  CENTRAL_CONFIG_SECURITY,
  CENTRAL_DISCOVER_USART_S,
  CENTRAL_DISCOVER_USART_C,
  CENTRAL_DONE,
  CENTRAL_STATE_NUM        // Position corresponding to the number of Central's states
} central_state_t;

/// Peripheral device states
typedef enum {
  PERIPHERAL_IDLE,
  PERIPHERAL_CENTRAL_CERT_OK,
  PERIPHERAL_SEND_PASSKEY,
  PERIPHERAL_INCREASE_SECURITY,
  PERIPHERAL_DONE,
  PERIPHERAL_STATE_NUM     // Position corresponding to the number of Peripheral's states
} peripheral_state_t;

/**************************************************************************//**
 * Proceed with execution. (Indicate that it is required to run the application
 * process action.)
 *****************************************************************************/
void app_proceed(void);

/**************************************************************************//**
 * Check if it is required to process with execution.
 * @return true if required, false otherwise.
 *****************************************************************************/
bool app_is_process_required(void);

/**************************************************************************//**
 * Acquire access to protected variables.
 *
 * Acquire the guard to operate on the internal state variables.
 * Guard is implemented using mutexing (RTOS).
 *
 * @note Must not be used from ISR context.
 *
 * @return true if operation was successful.
 *****************************************************************************/
bool app_mutex_acquire(void);

/**************************************************************************//**
 * Finish access to protected variables.
 *
 * Release the guard to stop working on the internal state variables.
 * Guard is implemented using mutexing (RTOS).
 *
 * @note Must not be used from ISR context.
 *****************************************************************************/
void app_mutex_release(void);

/**************************************************************************//**
 * Initialize the application.
 *
 * This function initializes the application components.
 *
 * @note Must not be used from ISR context.
 *****************************************************************************/
void app_init_bt(void);

#endif // APP_H
