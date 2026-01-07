/***************************************************************************//**
 * @file app_button_service.h
 * @brief Generic button service component for Simple button (Silabs)
 * @version 1.0.0
 * @details 
 * Provides a reusable button handling framework:
 * - Multiple button support
 * - Event callback system for application integration
 * - Optional mode-based button behavir
 * - Thread-safe design for interrupt context
 * @note This component is designed to be reusable across different projects
 *       by providing flexible configuration and callback mechanisms
*******************************************************************************/

#ifndef APP_BUTTON_SERVICE_H
#define APP_BUTTON_SERVICE_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "log.h"
#include "sl_simple_button_instances.h"
#include "sl_status.h"

/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/

// Maximum number of buttons supported by this service
#define BUTTON_SERVICE_MAX_BUTTONS  2

/*******************************************************************************
 *******************************   ENUMS   *************************************
 ******************************************************************************/

// Button ID (identifiers), can be extended for buttons
typedef enum
{
    BUTTON_ID_0 = 0,
    BUTTON_ID_1,
    BUTTON_ID_2,
    BUTTON_ID_3,
    BUTTON_ID_INVALID = 0xFF
}button_id_t;

// Button event types, 
typedef enum
{
    BUTTON_EVENT_PRESSED,   // Button was pressed
    BUTTON_EVENT_RELEASED,  // Button was released
    BUTTON_EVENT_HELD       // Button held for long duration (optional)
}button_event_type_t;

// Button service operating mode (for specific application)
typedef enum
{
    BUTTON_MODE_DISABLE = 0,
    BUTTON_MODE_NORMAL,
    BUTTON_MODE_PAIRING,    
    BUTTON_MODE_CUSTOM_     // Represent for specific application
}button_mode_t;

/*******************************************************************************
 *****************************   STRUCTURES   **********************************
 ******************************************************************************/

// Pack the button information to be sent/passed to callbacks
typedef struct 
{
  button_id_t button_id;           //< Which button triggered the event
  button_event_type_t event_type;  //< Type of button event
  button_mode_t current_mode;      //< Current button service mode
} button_event_t;

// Button configuration
typedef struct 
{
    bool enabled;      //< Is this button enabled?
}button_config_t;

/*******************************************************************************
 ***************************   CALLBACK TYPES   ********************************
 ******************************************************************************/

/**
 * @brief Button event callback
 * @param evt Pointer to button_event_t structure
 * @details
 * This callback is invoked whenever button events occur.
 * And is called from sl_button_on_change(), more flexible for various applications
 * instead ò being fixed inside.
 * @note This callback is called from interrupt context. Keep processing minimal.
 *       For lengthy operations, set a flag and process in main loop.
 */
typedef void (*button_event_callback_t)(const button_event_t *evt);

/*******************************************************************************
 ***************************   PUBLIC FUNCTIONS   ******************************
 ******************************************************************************/

/**
 * @brief Initialize the button service
 * @details
 * Initializes the button configuration to default values,
 * sets button's disabled mode and disable callbacks.
 * 
 * @return SL_STATUS_OK if successful
 * @return SL_STATUS_ALREADY_INITIALIZED if already initialized 
 */
sl_status_t button_service_init(void);

/**
 * @brief Confiure a particular button
 * @param button_id ID of the button to configure
 * @param config Pointer to button_config_t structure
 * 
 * @return SL_STATUS_OK if successful
 * @return SL_STATUS_INVALID_PARAMETER if button_id is invalid
 * @return SL_STATUS_NOT_INITIALIZED if service not initialized 
 */
sl_status_t button_service_configuration(button_id_t button_id,
                                         const button_config_t *config);

/**
 * @brief Resgister button event callback
 *  
 * @param callback Function pointer to the handler
 * @return SL_STATUS_OK if successful
 * @return SL_STATUS_NOT_INITIALIZED if service not initialized
 */
sl_status_t button_service_register_callback(button_event_callback_t callback);

/**
 * @brief Set button operating mode
 * @details
 * Change the operating mode of the button service. The callback
 * can check this mode to determine the appropriate action for button events.
 * 
 * @param mode Operating mode
 * @return SL_STATUS_OK if successful 
 */
sl_status_t button_service_set_mode(button_mode_t mode);

/**
 * @brief Get the current button operating mode
 * 
 * @return button_mode_t 
 */
button_mode_t button_service_get_mode(void);

/**
 * @brief Enable a specific button
 * 
 * @param button_id Button to enable
 * 
 * @return SL_STATUS_OK if successful
 * @return SL_STATUS_INVALID_PARAMETER if button_id is invalid
 */
sl_status_t button_service_enable_button(button_id_t button_id);

/**
 * @brief Disable a specific button
 * 
 * @param button_id Button to disable
 * 
 * @return SL_STATUS_OK if successful
 * @return SL_STATUS_INVALID_PARAMETER if button_id is invalid
 */
sl_status_t button_service_disable_button(button_id_t button_id);

/**
 * @brief Get current state of a button
 * 
 * @param[in] button_id Button to query
 * 
 * @return sl_button_sate_t
 */
sl_button_state_t button_service_get_button_state(button_id_t button_id);

/**
 * @brief Reset button service to default state
 * 
 * @details Disables all buttons and resets to initial configuration
 * 
 * @return SL_STATUS_OK if successful
 */
sl_status_t button_service_reset(void);

#endif
