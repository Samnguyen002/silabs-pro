/***************************************************************************//**
 * @file app_button_service.c
 * @brief Generic button service component for Simple button (Silabs)
 * @version 1.0.0
*******************************************************************************/

#include "app_button_service.h"
#include "app_button_service.h"
#include "sl_sleeptimer.h"
#include "app_assert.h"

#ifdef SL_COMPONENT_CATALOG_PRESENT
#include "sl_component_catalog.h"
#endif

// Button service initialization flag
static bool service_initialized = false;

// Button configurations
static button_config_t button_configs[BUTTON_SERVICE_MAX_BUTTONS];

// Current operating mode of the button
static button_mode_t current_mode = BUTTON_MODE_DISABLE;

static button_event_callback_t event_callback = NULL;

static const sl_button_t *button_instances[BUTTON_SERVICE_MAX_BUTTONS] = {
    &sl_button_btn0,
#if BUTTON_SERVICE_MAX_BUTTONS >=2
    &sl_button_btn1,
#endif
#if BUTTON_SERVICE_MAX_BUTTONS >=3
    &sl_button_btn2,
#endif
#if BUTTON_SERVICE_MAX_BUTTONS >=4
    &sl_button_btn3,
#endif
};

/*******************************************************************************
 ********************   LOCAL FUNCTIONS   **************************************
 ******************************************************************************/

static button_id_t get_button_id_from_handle(const sl_button_t *handle)
{
    for(int i = 0; i < BUTTON_SERVICE_MAX_BUTTONS; i++)
    {
        if(button_instances[i] != NULL)
        {
            if(button_instances[i] == handle)
            {
                return (button_id_t)i;
            }
        }
    }

    return BUTTON_ID_INVALID;
}

/*******************************************************************************
 ********************   GLOBAL FUNCTIONS   *************************************
 ******************************************************************************/

// Initialize the button service
sl_status_t button_service_init(void)
{
    if(service_initialized)
    {
        LOG_BUTTON(" Service already initialized");
        return SL_STATUS_ALREADY_INITIALIZED;
    }

    for(int i = 0; i < BUTTON_SERVICE_MAX_BUTTONS; i++)
    {
        button_configs[i].enabled = false;
    }
    
    service_initialized = true;
    current_mode = BUTTON_MODE_DISABLE;
    event_callback = NULL;

    LOG_BUTTON(" Service initialize completely");

    return SL_STATUS_OK;
}

// Configure a button
sl_status_t button_service_configuration(button_id_t button_id,
                                         const button_config_t *config)
{
    if(!service_initialized)
    {
       LOG_BUTTON("Service not initialized");
       return SL_STATUS_NOT_INITIALIZED;
    }

    if(button_id >= BUTTON_SERVICE_MAX_BUTTONS || config == NULL)
    {
        LOG_BUTTON("Invalid button ID %d", button_id);
        return SL_STATUS_INVALID_PARAMETER;
    }

    button_configs[button_id] = *config;
    LOG_BUTTON("Button %d, config %s",
                    button_id, config->enabled ? "ENABLED" : "DISABLED");

    return SL_STATUS_OK;
}

// Register a callback for the app's events
sl_status_t button_service_register_callback(button_event_callback_t callback)
{
    if(!service_initialized)
    {
       LOG_BUTTON("Service not initialized");
       return SL_STATUS_NOT_INITIALIZED;
    }

    event_callback = callback;
    LOG_BUTTON("Callback registered");

    return SL_STATUS_OK;
}

sl_status_t button_service_set_mode(button_mode_t mode)
{
    char *mode_name[] = {
        "DISABLE",
        "NORMAL",
        "PAIRING"
    };
    button_mode_t old_mode = current_mode;

    current_mode = mode;
    LOG_BUTTON("Changed button mode from %s to %s",
                mode_name[old_mode], mode_name[current_mode]);
    
    return SL_STATUS_OK;
}

button_mode_t button_service_get_mode(void)
{
    return current_mode;
}

sl_status_t button_service_enable_button(button_id_t button_id)
{
    if(button_id >= BUTTON_SERVICE_MAX_BUTTONS)
    {
        return SL_STATUS_INVALID_PARAMETER;
    }

    button_configs[button_id].enabled = true;

    return SL_STATUS_OK;
}

sl_status_t button_service_disable_button(button_id_t button_id)
{
    if(button_id >= BUTTON_SERVICE_MAX_BUTTONS)
    {
        return SL_STATUS_INVALID_PARAMETER;
    }

    button_configs[button_id].enabled = false;

    return SL_STATUS_OK;
}

sl_button_state_t button_service_get_button_state(button_id_t button_id)
{
    if(button_id >= BUTTON_SERVICE_MAX_BUTTONS)
    {
        LOG_BUTTON("Invalid button ID %d", button_id);
        return SL_SIMPLE_BUTTON_DISABLED;
    }

    return sl_button_get_state(button_instances[button_id]);
}

sl_status_t button_service_reset(void)
{
  LOG_BUTTON("Resetting button service");
  
  // Disable all buttons
  for(uint8_t i = 0; i < BUTTON_SERVICE_MAX_BUTTONS; i++) 
  {
    button_configs[i].enabled = false;
  }
    
  current_mode = BUTTON_MODE_DISABLE;
  
  return SL_STATUS_OK;
}

/*******************************************************************************
 ********************   BUTTON CALLBACK HANDLERS   *****************************
 ******************************************************************************/

void sl_button_on_change(const sl_button_t *handle)
{
    button_id_t button_id = get_button_id_from_handle(handle);

    if(!service_initialized) 
    {
        LOG_BUTTON("Service not initialized");
        return;
    }

    if(button_id == BUTTON_ID_INVALID) 
    {
        LOG_BUTTON("Unknown button handle");
        return;
    }

    if(!button_configs[button_id].enabled) 
    {
        LOG_BUTTON("Button %d is disabled, ignoring event", button_id);
        return;
    }

    if(current_mode == BUTTON_MODE_DISABLE) 
    {
        LOG_BUTTON(" Button service is disabled, ignoring event");
        return;
    }

    if(sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED)
    {
        button_event_t evt = {
            .button_id = button_id,
            .event_type = BUTTON_EVENT_PRESSED,
            .current_mode = current_mode
        };

        event_callback(&evt);

    #ifdef SL_CATALOG_BLUETOOTH_PRESENT
        LOG_BUTTON("Pressed");
    #endif
    }
    else if(sl_button_get_state(handle) == SL_SIMPLE_BUTTON_RELEASED)
    {
        button_event_t evt = {
            .button_id = button_id,
            .event_type = BUTTON_EVENT_RELEASED,
            .current_mode = current_mode
        };

        event_callback(&evt);
    #ifdef SL_CATALOG_BLUETOOTH_PRESENT
        LOG_BUTTON("Released");
    #endif       
    }
}