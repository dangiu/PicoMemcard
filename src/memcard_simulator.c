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

uint smSelMonitor;
uint smCmdReader;
uint smDatReader;
uint smDatWriter;

uint offsetSelMonitor;
uint offsetCmdReader;
uint offsetDatWriter;
uint offsetDatReader;

memory_card_t mc;
bool request_next_mc = false;
bool request_prev_mc = false;
bool request_new_mc = false;
mutex_t mutex_sm_tick;
queue_t mc_sector_sync_queue;

enum states {
	MC_IDLE,
	MC_COMMAND,
	MC_SEND_ID,
	MC_RECV_ADDR,
	MC_EXECUTE_READ,
	MC_EXECUTE_WRITE,
	MC_EXECUTE_ID,
	MC_ABORT,
	MC_END,
	PAD_ACCESS,
	PAD_SNIFF,
    MC_PRO_PING,
    MC_PRO_GAMEID
};

uint8_t current_state = MC_IDLE;
uint8_t next_state = MC_IDLE;
uint8_t command_state = MC_IDLE;
uint8_t checksum = 0x00;
uint8_t recv_checksum = 0x00;
uint8_t sm_byte_counter = 0;
sector_t sm_address = 0x0000;
uint16_t sw_status = 0x0000;	// pad switch status
uint8_t id_data[] = {MC_ACK1, MC_ACK2, 0x04, 0x00, 0x00, 0x80};

uint8_t byte_counter = 2;
uint8_t game_id_len = 0;
uint8_t game_id[255];

/**
 * @brief Simulates memory card being briefly unplugged and replugged
 */
void simulate_mc_reconnect() {
	pio_sm_set_enabled(pio0, smSelMonitor, false);
	pio_restart_sm_mask(pio0, 1 << smCmdReader | 1 << smDatReader | 1 << smDatWriter);
	pio_sm_exec(pio0, smCmdReader, pio_encode_jmp(offsetCmdReader));	// restart smCmdReader PC
	pio_sm_exec(pio0, smDatReader, pio_encode_jmp(offsetDatReader));	// restart smDatReader PC
	pio_sm_exec(pio0, smDatWriter, pio_encode_jmp(offsetDatWriter));	// restart smDatWriter PC
	pio_sm_clear_fifos(pio0, smCmdReader);
	pio_sm_clear_fifos(pio0, smDatReader);
	pio_sm_drain_tx_fifo(pio0, smDatWriter); // drain instead of clear, so that we empty the OSR
	printf("Simulating reconnection...\n");
	led_output_mc_change();
	sleep_ms(MC_RECONNECT_TIME);
	pio_sm_set_enabled(pio0, smSelMonitor, true);
}

void state_machine_tick(uint8_t data) {
	bool valid_command = false;
	current_state = next_state;

	switch(current_state) {
		case MC_IDLE: // idle / sleeping
			next_state = MC_IDLE;
			command_state = MC_IDLE;
			sm_byte_counter = 0;
			sm_address = 0x0000;
			checksum = 0x00;
			recv_checksum = 0x00;
			sw_status = 0x0000;
			switch(data) {
				case MEMCARD_TOP:
					// Send flag byte and start transaction
					SEND(mc.flag_byte);
					next_state = MC_COMMAND;
					break;
				case PAD_TOP:
					next_state = PAD_ACCESS;
					// fall through and cancel ack
				default:
                    break;
			}
			break;
		case PAD_ACCESS:
			switch(data) {
				case PAD_READ:
					next_state = PAD_SNIFF;
					break;
				default:
					next_state = MC_IDLE;
			}
			
			break;
		case PAD_SNIFF:
			switch (sm_byte_counter) {
				case 0:
					pio_sm_clear_fifos(pio0, smDatReader);	// clear out Hi-Z, idlo, and idhi bytes
					break;
				case 1: 
					sw_status = RECV_DAT();
					break;
				case 2:
					sw_status = sw_status | (RECV_DAT() << 8);
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
					break;
				default:
					next_state = MC_IDLE;
			}
			++sm_byte_counter;
			break;
		case MC_COMMAND: // received a wake up byte, wait for command
			switch(data) {
				case MEMCARD_READ:
					valid_command = true;
					command_state = MC_EXECUTE_READ;
					break;
				case MEMCARD_WRITE:
					valid_command = true;
					command_state = MC_EXECUTE_WRITE;
					break;
				case MEMCARD_ID:
					valid_command = true;
					command_state = MC_EXECUTE_ID;
					break;
        case MEMCARD_PING:
          valid_command = false;
          next_state = MC_PRO_PING;
          break;
        case MEMCARD_GAMEID:
          valid_command = false;
          next_state = MC_PRO_GAMEID;
          break;
				default:
					valid_command = false;
					next_state = MC_IDLE;
			}
			if (valid_command) {
				valid_command = false;
				next_state = MC_SEND_ID;
				SEND(MC_ID1);
			}
			break;
		case MC_SEND_ID:
			if (command_state == MC_EXECUTE_ID) {
				// ID doesn't need to receive an address
				next_state = command_state;
			} else {
				next_state = MC_RECV_ADDR;
			}
			SEND(MC_ID2);
			break;
		case MC_RECV_ADDR: // receive the address
			if (sm_byte_counter == 0) {
				// Filler
				SEND(0x00);
				sm_byte_counter++;
			} else if (sm_byte_counter == 1) {
				// MSB
				sm_address = data << 8;
				// Send MSB
				SEND(data);
				sm_byte_counter++;
			} else if (sm_byte_counter == 2) {
				// LSB
				sm_address |= data;
				if(command_state == MC_EXECUTE_READ) {
					SEND(MC_ACK1);
				} else {
					// Otherwise send LSB
                    SEND(data);
				}

				next_state = command_state;
				command_state = MC_IDLE;
				sm_byte_counter = 0;
			}
			break;
		case MC_EXECUTE_ID: // send mc id - used to identify which type of device this is
			if(sm_byte_counter < sizeof(id_data)) {
                SEND(id_data[sm_byte_counter++]);
			} else {
				next_state = MC_IDLE;
			}
			break;
		case MC_EXECUTE_READ: // do a read operation
			if(sm_byte_counter == 0) {
				// Send ACK2
                SEND(MC_ACK2);
				checksum = ((sm_address & 0xFF00) >> 8) ^ (sm_address & 0x00FF);
			} else if (sm_byte_counter > 0 && sm_byte_counter < 3) {
				if(memory_card_is_sector_valid(&mc, sm_address)) {
					if (sm_byte_counter == 1) {
						// MSB
                        SEND((sm_address & 0xFF00) >> 8);
					} else {
						// LSB
                        SEND((sm_address & 0x00FF));
					}
				} else {
					// Abort transaction - invalid sector
                    SEND(0xff);
					next_state = MC_ABORT;
				}
			} else {
				// Performing read
				// byte counter is 3 at start here
				uint8_t* sec_ptr = memory_card_get_sector_ptr(&mc, sm_address);
				if ((sm_byte_counter - 3) < MC_SEC_SIZE) {
                    SEND(sec_ptr[sm_byte_counter - 3]);
					checksum ^= sec_ptr[sm_byte_counter - 3];
				} else {
					// Send checksum
                    SEND(checksum);
					checksum = 0x00;
					next_state = MC_END;
				}
			}
			sm_byte_counter++;
			break;
		case MC_EXECUTE_WRITE: // do a write operation
			if(memory_card_is_sector_valid(&mc, sm_address)) {
				uint8_t* sec_ptr = memory_card_get_sector_ptr(&mc, sm_address);
				if(sm_byte_counter == 0) {
					checksum = ((sm_address & 0xFF00) >> 8) ^ (sm_address & 0x00FF);
				}
				if(sm_byte_counter < MC_SEC_SIZE) {
					checksum ^= data;
					sec_ptr[sm_byte_counter] = data;
                    SEND(data);
				} else {
					if (sm_byte_counter == MC_SEC_SIZE) {
						// Read checksum
						recv_checksum = data;
                        SEND(MC_ACK1);
					} else {
						// ACK 2
                        SEND(MC_ACK2);
						memory_card_reset_seen_flag(&mc);
						if(sm_address != MC_TEST_SEC) {
							queue_add_blocking(&mc_sector_sync_queue, &sm_address);
						}
						next_state = MC_END;
					}
				}
			} else {
                SEND(0xff);
				next_state = MC_ABORT;
			}
			sm_byte_counter++;
			break;
		case MC_ABORT: // something went wrong, abort
            SEND(0xff);
			next_state = MC_IDLE;
			break;
		case MC_END: // end
			// Send end byte and update timestamp
			if(recv_checksum == checksum) {
                SEND(MC_GOOD);
			} else {
                SEND(MC_BAD_CHK);
			}
			next_state = MC_IDLE;
			break;
    case MC_PRO_PING:
      if (byte_counter & 2) { // 2 or 3 (RESERVED)
          SEND(0x00);
      }
      if (byte_counter == 4) {
          SEND(0x27); // Card present
      }
      if (byte_counter == 5) {
        printf("MC Received Ping from PS\n");
        byte_counter = 2;
        next_state = MC_IDLE;
        break;
      }
      byte_counter++;
      break;
    case MC_PRO_GAMEID:
      if (byte_counter == 2) { // First byte (RESERVED)
          SEND(0x00);
      }
      else if (byte_counter == 3) { // Length
        game_id_len = data; // Note: if data is 255, this could overflow our string by one byte with the null char. TODO: Add sanity check (eg. length is 0)
          SEND(0x00);
      }
      else if ((byte_counter - 3) < game_id_len) { // ...bytes...
        game_id[byte_counter - 4] = data;
          SEND(data);
      }
      else { // Last byte
        game_id[byte_counter - 4] = data;
        game_id[byte_counter - 3] = 0; // GAMEID string should be null terminated already, but we shouldn't trust it.
        printf("Game ID: %s\n", game_id);
        byte_counter = 2;
        next_state = MC_IDLE;
        break;
      }
      byte_counter++;
      break;
		default:
			next_state = MC_IDLE;
            SEND(0xff);
	}
}

_Noreturn void simulation_thread() {
	printf("Simulation core begin...\n");
	while(true) {
		mutex_enter_blocking(&mutex_sm_tick);
		uint8_t item = RECV_CMD();
		state_machine_tick(item);
		mutex_exit(&mutex_sm_tick);
	}
}

bool is_mc_switch_safe() {
	return (current_state != MC_EXECUTE_WRITE && next_state != MC_EXECUTE_WRITE && queue_is_empty(&mc_sector_sync_queue));
}

void __time_critical_func(restart_pio_sm()) {
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

    // TODO remove this after switching back to separate handling methods
    // Reset mc state machine
    current_state = MC_IDLE;
    next_state = MC_IDLE;
    command_state = MC_IDLE;
    sm_byte_counter = 0;
    sm_address = 0x0000;
    checksum = 0x00;
    recv_checksum = 0x00;
    sw_status = 0x0000;

    multicore_launch_core1(simulation_thread);
    pio_enable_sm_mask_in_sync(pio0, 1 << smCmdReader | 1 << smDatWriter);

    pio_enable_sm_mask_in_sync(pio0, 1 << smCmdReader | 1 << smDatReader | 1 << smDatWriter);
}

void init_pio() {
    gpio_set_dir(PIN_DAT, 0);
    gpio_set_dir(PIN_CMD, 0);
    gpio_set_dir(PIN_SEL, 0);
    gpio_set_dir(PIN_CLK, 0);
    gpio_set_dir(PIN_ACK, 0);
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

_Noreturn int simulate_memory_card() {
	mutex_init(&mutex_sm_tick);
	queue_init(&mc_sector_sync_queue, sizeof(sector_t), MC_SEC_COUNT);	// enough space to do complete MC copy
	uint8_t mc_file_name[MAX_MC_FILENAME_LEN + 1];	// +1 for null terminator character

	/* Mount and test SD card filesystem */
	sd_card_t *p_sd = sd_get_by_num(0);
	if(FR_OK != f_mount(&p_sd->fatfs, "", 1)) {
		while(true)
			led_blink_error(1);
	}

	uint32_t status;	
	status = memory_card_init(&mc);
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
	multicore_launch_core1(simulation_thread);

	while(true) {
		if(!queue_is_empty(&mc_sector_sync_queue)) {
			led_output_sync_status(true);
			uint16_t next_entry;
			queue_remove_blocking(&mc_sector_sync_queue, &next_entry);
			status = memory_card_sync_sector(&mc, next_entry, mc_file_name);
			if(status != MC_OK)
				led_blink_error(status);
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
					if(is_mc_switch_safe()) {	// check that switch is safe before getting the lock
						mutex_enter_blocking(&mutex_sm_tick);
						if(is_mc_switch_safe()) {	// and also after
							strcpy(mc_file_name, new_file_name);
							status = memory_card_import(&mc, mc_file_name);
							if(status != MC_OK)
								led_blink_error(status);
							simulate_mc_reconnect();
							request_next_mc = false;
							request_prev_mc = false;
						}
						mutex_exit(&mutex_sm_tick);
					}
				}
			}
		} else if(request_new_mc) {
			if(is_mc_switch_safe()) {	// check that switch is safe before getting the lock
				mutex_enter_blocking(&mutex_sm_tick);
				if(is_mc_switch_safe()) {	// and also after
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
				}
				mutex_exit(&mutex_sm_tick);
			}
		}
	}
}