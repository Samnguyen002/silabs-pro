/***************************************************************************//**
 * @file app_button_service.c
 * @brief Button-based pairing control component for BLE applications
 * @version 1.0.0
*******************************************************************************/

#include "app_button_pairing_complete.h"

void app_button_pairing_init(button_event_callback_t button_pairing_hanlder)
{
    sl_status_t sc;
    button_config_t btn0_config = {
        .enabled = false,
    };
    button_config_t btn1_config = {
        .enabled = false,
    };

    sc = button_service_init();
    if(sc != SL_STATUS_OK)
    {
        LOG_PAIRING("Failed to initialize button service");
        return;
    }

    sc = button_service_configuration(BUTTON_ID_0, 
                                      &btn0_config);
    if(sc != SL_STATUS_OK)
    {
        LOG_PAIRING("Failed to configure button 0");
        return;
    }

    sc = button_service_configuration(BUTTON_ID_1, 
                                      &btn1_config);
    if(sc != SL_STATUS_OK)
    {
        LOG_PAIRING("Failed to configure button 1");
        return;
    }
    
    sc = button_service_register_callback(button_pairing_hanlder);
    if(sc != SL_STATUS_OK)
    {
        LOG_PAIRING("Failed to register callback");
        return;
    }
}

void app_button_pairing_enable(void)
{
  button_service_enable_button(BUTTON_ID_0);
  button_service_enable_button(BUTTON_ID_1);
  button_service_set_mode(BUTTON_MODE_PAIRING);  

  LOG_PAIRING("Enabling pairing button mode");
  LOG_PAIRING("Press BTN0 to CONFIRM or BTN1 to REJECT");
}

void app_button_pairing_disable(void)
{
  // Disable both buttons
  button_service_disable_button(BUTTON_ID_0);
  button_service_disable_button(BUTTON_ID_1);
  
  // Set mode back to disabled
  button_service_set_mode(BUTTON_MODE_DISABLE);

  LOG_PAIRING("Disabling pairing button mode");
}