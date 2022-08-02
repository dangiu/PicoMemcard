#include <string.h>
#include "sd_config.h"
#include "ff.h" 
#include "diskio.h"
#include "config.h"

void spi0_dma_isr();

static spi_t spis[] = {
{
    .hw_inst = spi0,
    .miso_gpio = PIN_MISO,
    .mosi_gpio = PIN_MOSI,
    .sck_gpio = PIN_SCK,
    .baud_rate = BAUD_RATE,   

    .dma_isr = spi0_dma_isr
}};

void spi0_dma_isr() { spi_irq_handler(&spis[0]); }

static sd_card_t sd_cards[] = {
    {
        .pcName = "0:",
        .spi = &spis[0],
        .ss_gpio = PIN_SS,
        .use_card_detect = false,
        .m_Status = STA_NOINIT
    }
};

size_t sd_get_num() { return count_of(sd_cards); }
sd_card_t *sd_get_by_num(size_t num) {
    if (num <= sd_get_num()) {
        return &sd_cards[num];
    } else {
        return NULL;
    }
}
size_t spi_get_num() { return count_of(spis); }
spi_t *spi_get_by_num(size_t num) {
    if (num <= sd_get_num()) {
        return &spis[num];
    } else {
        return NULL;
    }
}