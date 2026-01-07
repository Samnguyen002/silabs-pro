#include "burtc.h"

/**
 * @brief Initialize the BURTC (backup real-time counter) module.
 *
 * Enables the BURTC clock, initializes the module with default parameters,
 * enables the module and starts the counter.
 */
void init_burtc(void)
{
    // Enable clock for BURTC
    CMU_ClockEnable(cmuClock_BURTC, true);

    // Use default initialization configuration
    // #define SL_HAL_BURTC_INIT_DEFAULT \ { \ 1, \ false, \ false, \ false, \ false, \ }
    sl_hal_burtc_init_t init = SL_HAL_BURTC_INIT_DEFAULT;
    // If desired, customize init.prescaler, init.clockSelect, ...
    sl_hal_burtc_init(&init);

    // Enable module
    sl_hal_burtc_enable();

    // Start counter
    sl_hal_burtc_start();
}

/**
 * @brief Read current BURTC counter value.
 *
 * Waits for synchronization if necessary and returns the current counter.
 * @return Current BURTC counter value
 */
uint32_t get_burtc_count(void)
{
    // Wait for synchronization if required
    sl_hal_burtc_wait_sync();
    uint32_t count = sl_hal_burtc_get_counter();
    return count;
}

/**
 * @brief Convert a raw counter value to seconds.
 *
 * This helper converts the raw BURTC counter to seconds using the provided
 * ticks-per-second parameter (e.g., 32768 for LFRCO without prescaler).
 *
 * @param count Raw BURTC counter value
 * @param ticks_per_second Number of counter ticks per second
 * @return Equivalent number of seconds
 */
uint32_t convert_count_to_seconds(uint32_t count, uint32_t ticks_per_second)
{
    return count / ticks_per_second;
}
