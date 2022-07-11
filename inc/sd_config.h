#pragma once

#include "ff.h"
#include "sd_card.h"    
    
#ifdef __cplusplus
extern "C" {
#endif

    size_t sd_get_num();
    sd_card_t *sd_get_by_num(size_t num);
    
    size_t spi_get_num();
    spi_t *spi_get_by_num(size_t num);

#ifdef __cplusplus
}
#endif
