#include <string.h>
#include "memcard_simulator.h"
#include "stdio.h"
#include "pico/multicore.h"
#include "pico/platform.h"  // __time_critical_func macro
#include "pico/util/queue.h"
#include "hardware/pio.h"
#include "hardware/irq.h"
#include "psxSPI.pio.h"
#include "memory_card.h"
#include "sd_config.h"
#include "memcard_manager.h"
#include "config.h"
#include "pad.h"
#include "led.h"

#define MEMCARD_TOP 0x81
#define MEMCARD_READ 0x52
#define MEMCARD_WRITE 0x57
#define MEMCARD_ID 0x53

#define MEMCARD_PING 0x20
#define MEMCARD_GAMEID 0x21
#define MEMCARD_PREV_CHAN 0x22
#define MEMCARD_NEXT_CHAN 0x23
#define MEMCARD_PREV_CARD 0x24
#define MEMCARD_NEXT_CARD 0x25
#define MEMCARD_NAME 0x26

#define PAD_TOP 0x01
#define PAD_READ 0x42

#define SEND(byte) write_byte_blocking(pio0, smDatWriter, byte)
#define ACK() SEND(0xff)    // ACK without sending anything by keeping the DAT line always high
#define RECV_CMD() read_byte_blocking(pio0, smCmdReader)
#define RECV_DAT() read_byte_blocking(pio0, smDatReader)

uint smCmdReader;
uint smDatReader;
uint smDatWriter;

uint offsetCmdReader;
uint offsetDatWriter;
uint offsetDatReader;

memory_card_t mc;
bool request_next_mc = false;
bool request_prev_mc = false;
bool request_new_mc = false;
mutex_t write_transaction;
queue_t mc_sector_sync_queue;
const uint8_t id_data[] = {0x04, 0x00, 0x00, 0x80};

void simulate_mc_reconnect() {
    irq_set_enabled(IO_IRQ_BANK0, false);
	pio_restart_sm_mask(pio0, 1 << smCmdReader | 1 << smDatReader | 1 << smDatWriter);
	pio_sm_exec(pio0, smCmdReader, pio_encode_jmp(offsetCmdReader));	// restart smCmdReader PC
	pio_sm_exec(pio0, smDatReader, pio_encode_jmp(offsetDatReader));	// restart smDatReader PC
	pio_sm_exec(pio0, smDatWriter, pio_encode_jmp(offsetDatWriter));	// restart smDatWriter PC
	pio_sm_clear_fifos(pio0, smCmdReader);
	pio_sm_clear_fifos(pio0, smDatReader);
	pio_sm_drain_tx_fifo(pio0, smDatWriter); // drain instead of clear, so that we empty the OSR
	printf("Simulating reconnection...");
	led_output_mc_change();
	sleep_ms(MC_RECONNECT_TIME);
    printf("  done\n");
    irq_set_enabled(IO_IRQ_BANK0, true);
}

void process_memcard_cmd() {
    SEND(mc.flag_byte);
    uint8_t data = RECV_CMD();
    switch(data) {
        case MEMCARD_READ:
            {
                SEND(MC_ID1);
                RECV_CMD(); // discard 0-filled data
                SEND(MC_ID2);
                RECV_CMD();
                SEND(0x00); // send filler
                data = RECV_CMD();
                sector_t read_address = data << 8;
                SEND(data); // confirm received MSB
                data = RECV_CMD();
                read_address |= data;
                SEND(MC_ACK1);
                RECV_CMD();
                SEND(MC_ACK2);
                RECV_CMD();
                uint8_t checksum = ((read_address & 0xFF00) >> 8) ^ (read_address & 0x00FF);
                if(!memory_card_is_sector_valid(&mc, read_address)) {
                    SEND(0xff); // abort transaction
                    return;
                }
                SEND((read_address & 0xFF00) >> 8); // confirm MSB
                RECV_CMD();
                SEND(read_address & 0x00FF);    // confirm LSB
                RECV_CMD();

                /* send data */
                uint8_t* sec_ptr = memory_card_get_sector_ptr(&mc, read_address);
                for (uint32_t i = 0; i < MC_SEC_SIZE; i++) {
                    SEND(sec_ptr[i]);
                    RECV_CMD();
                    checksum ^= sec_ptr[i];
                }
                SEND(checksum);
                RECV_CMD();
                SEND(MC_GOOD);
                RECV_CMD();
            }
            break;
        case MEMCARD_WRITE:
            {
                mutex_enter_blocking(&write_transaction);
                SEND(MC_ID1);
                RECV_CMD();
                SEND(MC_ID2);
                RECV_CMD();
                SEND(0x00);
                data = RECV_CMD();
                sector_t write_address = data << 8;
                SEND(data); // confirm received MSB
                data = RECV_CMD();
                write_address |= data;
                uint8_t checksum = ((write_address & 0xFF00) >> 8) ^ (write_address & 0x00FF);
                if(!memory_card_is_sector_valid(&mc, write_address)) {
                    SEND(0xff); // abort transaction
                    return;
                }
                uint8_t* sec_ptr = memory_card_get_sector_ptr(&mc, write_address);
                for(uint32_t i = 0; i < MC_SEC_SIZE; i++) {
                    SEND(data); // ack previous data
                    data = RECV_CMD();  // receive new data
                    checksum ^= data;
                    sec_ptr[i] = data;
                }
                SEND(data); // send remaining sector byte
                uint8_t recv_checksum = RECV_CMD();
                /* send acks */
                SEND(MC_ACK1);
                RECV_CMD();
                SEND(MC_ACK2);
                RECV_CMD();
                memory_card_reset_seen_flag(&mc);
                if(write_address != MC_TEST_SEC) {
                    queue_add_blocking(&mc_sector_sync_queue, &write_address);
                }
                if(checksum == recv_checksum)
                    SEND(MC_GOOD);
                else
                    SEND(MC_BAD_CHK);
                RECV_CMD();
                mutex_exit(&write_transaction);
            }
            break;
        case MEMCARD_ID:
            {
                SEND(MC_ID1);
                RECV_CMD();
                SEND(MC_ID2);
                RECV_CMD();
                SEND(0x00);
                /* send acks */
                SEND(MC_ACK1);
                RECV_CMD();
                SEND(MC_ACK2);
                RECV_CMD();
                for(uint32_t i = 0; i < sizeof(id_data); i++) {
                    SEND(id_data[i]);
                    RECV_CMD();
                }
            }
            break;
        case MEMCARD_PING:
            {
                SEND(0x00); // byte 1 reserved
                RECV_CMD();
                SEND(0x00); // byte 2 reserved
                RECV_CMD();
                SEND(0x27); // card present
                RECV_CMD();
                printf("MC Received Ping from PS\n");
            }
            break;
        case MEMCARD_GAMEID:
            {
                SEND(0x00);
                uint8_t game_id_len = RECV_CMD();
                data = 0x00;
                uint8_t game_id[256] = {0};
                /* read game id */
                for(uint32_t i = 0; i < game_id_len; i++) {
                    SEND(data); // ack previous data
                    data = RECV_CMD();
                    game_id[i] = data;
                }
                SEND(data); // ack last byte
                printf("Game ID: %s\n", game_id);
            }
            break;
        default:
            break;
    }
}

void process_pad_cmd() {    /* during pad interaction never call SEND() only interested in listening passively */
    if(RECV_CMD() != PAD_READ)  // only interested in PSX trying to read pad
        return;
    RECV_CMD(); // ignore TAP byte
    pio_sm_clear_fifos(pio0, smDatReader);	// clear out Hi-Z, idlo, and idhi bytes
    uint16_t sw_status = RECV_DAT();
    sw_status |= RECV_DAT() << 8;
    switch(sw_status) {
        case START & SELECT & UP:
            request_next_mc = true;
            break;
        case START & SELECT & DOWN:
            request_prev_mc = true;
            break;
        case START & SELECT & TRIANGLE:
            request_new_mc = true;
            break;
        default:
            break;
    }
}

void process_cmd(uint8_t cmd) {
    switch (cmd) {
        case MEMCARD_TOP:
            process_memcard_cmd();
            break;
        case PAD_TOP:
            process_pad_cmd();
            break;
        default:
            break;
    }
}

_Noreturn void simulation_thread() {
	while(true) {
        process_cmd(RECV_CMD());
	}
}

void __time_critical_func(restart_pio_sm)(void) {
    pio_set_sm_mask_enabled(pio0, 1 << smCmdReader | 1 << smDatReader | 1 << smDatWriter, false);
    pio_restart_sm_mask(pio0, 1 << smCmdReader | 1 << smDatReader | 1 << smDatWriter);
    pio_sm_exec(pio0, smCmdReader, pio_encode_jmp(offsetCmdReader));	// restart smCmdReader PC
    pio_sm_exec(pio0, smDatReader, pio_encode_jmp(offsetDatReader));	// restart smDatReader PC
    pio_sm_exec(pio0, smDatWriter, pio_encode_jmp(offsetDatWriter));	// restart smDatWriter PC
    pio_sm_clear_fifos(pio0, smCmdReader);
    pio_sm_clear_fifos(pio0, smDatReader);
    pio_sm_drain_tx_fifo(pio0, smDatWriter); // drain instead of clear, so that we empty the OSR

    // resetting and launching core1 here allows to perform the reset of the transaction (e.g. when PSX polls for new MC without completing the read)
    multicore_reset_core1();
    multicore_launch_core1(simulation_thread);
    pio_enable_sm_mask_in_sync(pio0, 1 << smCmdReader | 1 << smDatReader | 1 << smDatWriter);
}

void init_pio() {
    gpio_set_dir(PIN_DAT, false);
    gpio_set_dir(PIN_CMD, false);
    gpio_set_dir(PIN_SEL, false);
    gpio_set_dir(PIN_CLK, false);
    gpio_set_dir(PIN_ACK, false);
    gpio_disable_pulls(PIN_DAT);
    gpio_disable_pulls(PIN_CMD);
    gpio_disable_pulls(PIN_SEL);
    gpio_disable_pulls(PIN_CLK);
    gpio_disable_pulls(PIN_ACK);

    smCmdReader = pio_claim_unused_sm(pio0, true);
    smDatReader = pio_claim_unused_sm(pio0, true);
    smDatWriter = pio_claim_unused_sm(pio0, true);

    offsetCmdReader = pio_add_program(pio0, &cmd_reader_program);
    offsetDatReader = pio_add_program(pio0, &dat_reader_program);
    offsetDatWriter = pio_add_program(pio0, &dat_writer_program);

    cmd_reader_program_init(pio0, smCmdReader, offsetCmdReader);
    dat_reader_program_init(pio0, smDatReader, offsetDatReader);
    dat_writer_program_init(pio0, smDatWriter, offsetDatWriter);
}

void __time_critical_func(sel_isr_callback()) {
    // TODO refractor comment, also is __time_critical_func needed for speed? we should test if everything works without it!
    /* begin inlined call of:  gpio_acknowledge_irq(PIN_SEL, GPIO_IRQ_EDGE_RISE); kept in RAM for performance reasons */
    check_gpio_param(PIN_SEL);
    iobank0_hw->intr[PIN_SEL / 8] = GPIO_IRQ_EDGE_RISE << (4 * (PIN_SEL % 8));
    /* end of inlined call */
    restart_pio_sm();
}

void queue_sync_step(queue_t* queue, uint8_t* mc_file_name) {
    uint16_t next_entry;
    queue_remove_blocking(queue, &next_entry);
    uint32_t status = memory_card_sync_sector(&mc, next_entry, mc_file_name);
    if(status != MC_OK)
        led_blink_error(status);
}

_Noreturn int simulate_memory_card() {
	mutex_init(&write_transaction);
	queue_init(&mc_sector_sync_queue, sizeof(sector_t), MC_SEC_COUNT);	// enough space to do complete MC copy
	uint8_t mc_file_name[MAX_MC_FILENAME_LEN + 1];	// +1 for null terminator character

	/* Mount and test SD card filesystem */
	sd_card_t *p_sd = sd_get_by_num(0);
	if(FR_OK != f_mount(&p_sd->fatfs, "", 1)) {
		while(true)
			led_blink_error(1);
	}

    uint32_t status = memory_card_init(&mc);
	if(status != MC_OK) {
		while(true) {
			led_blink_error(status);
			sleep_ms(2000);
		}
	}
	status = memcard_manager_get_initial(mc_file_name);	// get initial memory card to load
	if(status != MM_OK) {
		status = memcard_manager_get(0, mc_file_name);	// revert to first mem card if failing to load previously loaded card
		if(status != MM_OK) {
			while(true) {
				led_blink_error(status);
				sleep_ms(1000);
			}
		}
	}
	status = memory_card_import(&mc, mc_file_name);
	if(status != MC_OK) {
		while(true) {
			led_blink_error(status);
			sleep_ms(2000);
		}
	}

    printf("Initializing PIO...");
    init_pio();
    printf("  done\n");

    /* Setup SEL interrupt on GPIO */
    // gpio_set_irq_enabled_with_callback(PIN_SEL, GPIO_IRQ_EDGE_RISE, true, my_gpio_callback);  // decomposed into:
    gpio_set_irq_enabled(PIN_SEL, GPIO_IRQ_EDGE_RISE, true);
    irq_set_exclusive_handler(IO_IRQ_BANK0, sel_isr_callback); // instead of normal gpio_set_irq_callback() which has slower handling
    irq_set_enabled(IO_IRQ_BANK0, true);

    /* Setup additional GPIO configuration options */
    gpio_set_slew_rate(PIN_DAT, GPIO_SLEW_RATE_FAST);
    gpio_set_drive_strength(PIN_DAT, GPIO_DRIVE_STRENGTH_12MA);

    /* SMs are automatically enabled on first SEL reset */

	/* Launch memory card thread */
    printf("Starting simulation core...");
	multicore_launch_core1(simulation_thread);
    printf("  done\n");

    /* Process sync/switch/creation requests */
	while(true) {
		if(!queue_is_empty(&mc_sector_sync_queue)) {
			led_output_sync_status(true);
            queue_sync_step(&mc_sector_sync_queue, mc_file_name);
		} else {
			led_output_sync_status(false);
		}
		if(request_next_mc || request_prev_mc) {
			if(request_next_mc && request_prev_mc) {
				/* requested change in both directions, do nothing */
				request_next_mc = false;
				request_prev_mc = false;
			} else {
				uint8_t new_file_name[MAX_MC_FILENAME_LEN + 1];
				if(request_next_mc)
					status = memcard_manager_get_next(mc_file_name, new_file_name);
				else if (request_prev_mc)
					status = memcard_manager_get_prev(mc_file_name, new_file_name);
				if(status != MM_OK) {
					led_output_end_mc_list();
					request_next_mc = false;
					request_prev_mc = false;
				} else {
                    mutex_enter_blocking(&write_transaction);
                    /* ensure latest write operations have been synced */
                    led_output_sync_status(true);
                    while(!queue_is_empty(&mc_sector_sync_queue))
                        queue_sync_step(&mc_sector_sync_queue, mc_file_name);
                    led_output_sync_status(false);
                    /* switch mc */
                    strcpy(mc_file_name, new_file_name);
                    status = memory_card_import(&mc, mc_file_name);
                    if(status != MC_OK)
                        led_blink_error(status);
                    simulate_mc_reconnect();
                    request_next_mc = false;
                    request_prev_mc = false;
                    mutex_exit(&write_transaction);
				}
			}
		} else if(request_new_mc) {
				mutex_enter_blocking(&write_transaction);
                /* ensure latest write operations have been synced */
                led_output_sync_status(true);
                while(!queue_is_empty(&mc_sector_sync_queue))
                    queue_sync_step(&mc_sector_sync_queue, mc_file_name);
                led_output_sync_status(false);
                /* create new mc */
                uint8_t new_name[MAX_MC_FILENAME_LEN + 1];
                status = memcard_manager_create(new_name);
                if(status == MM_OK) {
                    led_output_new_mc();
                    strcpy(mc_file_name, new_name);
                    status = memory_card_import(&mc, mc_file_name);	// switch to newly created mc image
                    if(status != MC_OK)
                        led_blink_error(status);
                } else
                    led_blink_error(status);
                simulate_mc_reconnect();
                request_new_mc = false;
                mutex_exit(&write_transaction);
		}
	}
}