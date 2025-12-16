/***************************************************************************//**
 * @file app_button_pairing_complete.h
 * @brief Button-based pairing control component for BLE applications
 * @version 1.0.0
 * @details
 * Provides button-triggered pairing functionality for Bluetooth Low Energy devices:
 * - Button event handling for pairing initiation
 * - Pairing mode control and state management
 * - Flexible callback registration for application-specific pairing logic
 * - Thread-safe design suitable for interrupt-driven button events
 * @note This component integrates with the app_button_service framework,
 *       enabling seamless button event routing to pairing control logic
*******************************************************************************/

#ifndef APP_BUTTON_PAIRING_COMPLETE_H
#define APP_BUTTON_PAIRING_COMPLETE_H

#include "app_button_service.h"

/*******************************************************************************
 ***************************   PUBLIC FUNCTIONS   ******************************
 ******************************************************************************/

/**
 * @brief Initialize button-based pairing functionality
 * @param pairing_button_hanlder Callback function invoked on button events
 * @details
 * Sets up the pairing control system by registering a callback to handle
 * button events. The callback will be invoked whenever a button event occurs,
 * allowing the application to respond with appropriate pairing actions by using 
 * button (e.g., enter pairing mode, exit pairing mode, etc.).
 * @note Must be called during application initialization before entering
 *       the main event loop
 * @warning Keep the callback function minimal; complex operations should be
 *          deferred to the main application loop
 */
void app_button_pairing_init(button_event_callback_t button_pairing_hanlder);

/**
 * @brief Enable button-triggered pairing mode
 * @details
 * Activates the pairing control system, allowing button presses to trigger
 * pairing-related actions. After calling this function, button events will
 * be processed and routed to the registered pairing callback.
 * @note Call this function to activate button functionality after
 *       initialization or when transitioning to a pairing-enabled state
 */
void app_button_pairing_enable(void);

/**
 * @brief Disable button-triggered pairing mode
 * @details
 * Deactivates the pairing control system, preventing button presses from
 * triggering pairing actions. Button events will not be processed after
 * this function is called. Use this to temporarily suspend pairing
 * functionality or when exiting pairing mode.
 * @note Call this function to deactivate pairing when transitioning away
 *       from a pairing-enabled state
 */
void app_button_pairing_disable(void);

#endif // APP_BUTTON_PAIRING_COMPLETE_H