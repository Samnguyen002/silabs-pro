#ifndef BURTC_H
#define BURTC_H

#include "sl_hal_burtc.h"
#include "em_cmu.h"
#include "em_assert.h"

void init_burtc(void);
uint32_t get_burtc_count(void);
uint32_t convert_count_to_seconds(uint32_t count, uint32_t ticks_per_second);

#endif 