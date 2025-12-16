#include "burtc.h"
// Nếu cần convert counter thành ngày giờ bạn sẽ cần logic riêng.
// Ở đây chúng thiết lập đơn giản và đọc raw count.

// Khởi BURTC
void init_burtc(void)
{
    // Bật clock cho BURTC
    CMU_ClockEnable(cmuClock_BURTC, true);

    // Chuẩn cấu hình init mặc định
    // #define SL_HAL_BURTC_INIT_DEFAULT \ { \ 1, \ false, \ false, \ false, \ false, \ }
    sl_hal_burtc_init_t init = SL_HAL_BURTC_INIT_DEFAULT;
    // Nếu bạn muốn, bạn có thể cấu hình init.prescaler, init.clockSelect, ...
    sl_hal_burtc_init(&init);

    // Bật module
    sl_hal_burtc_enable();

    // Khởi đếm
    sl_hal_burtc_start();
}

// Đọc counter hiện tại
uint32_t get_burtc_count(void)
{
    // Đồng bộ trước nếu cần
    sl_hal_burtc_wait_sync();
    uint32_t count = sl_hal_burtc_get_counter();
    return count;
}

// Chuyển counter thành số giây (giả sử prescaler/tần số biết rõ)
// prescaler = 1; clock = LFRCO = 32768 Hz (thường là vậy) (32768 ticks/giây)
uint32_t convert_count_to_seconds(uint32_t count, uint32_t ticks_per_second)
{
    return count / ticks_per_second;
}
