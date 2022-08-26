#include <string.h>
#include "memcard_simulator.h"
#include "stdio.h"
#include "stdlib.h"
#include "pico/multicore.h"
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

#define PAD_TOP 0x01
#define PAD_READ 0x42

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

void restart_pio_sm() {
	pio_set_sm_mask_enabled(pio0, 1 << smCmdReader | 1 << smDatReader | 1 << smDatWriter, false);
	pio_restart_sm_mask(pio0, 1 << smCmdReader | 1 << smDatReader | 1 << smDatWriter);
	pio_sm_exec(pio0, smCmdReader, pio_encode_jmp(offsetCmdReader));	// restart smCmdReader PC
	pio_sm_exec(pio0, smDatReader, pio_encode_jmp(offsetDatReader));	// restart smDatReader PC
	pio_sm_exec(pio0, smDatWriter, pio_encode_jmp(offsetDatWriter));	// restart smDatWriter PC
	pio_sm_clear_fifos(pio0, smCmdReader);
	pio_sm_clear_fifos(pio0, smDatReader);
	pio_sm_drain_tx_fifo(pio0, smDatWriter); // drain instead of clear, so that we empty the OSR

	// Reset mc state machine
	current_state = MC_IDLE;
	next_state = MC_IDLE;
	command_state = MC_IDLE;
	sm_byte_counter = 0;
	sm_address = 0x0000;
	checksum = 0x00;
	recv_checksum = 0x00;
	sw_status = 0x0000;

	pio_enable_sm_mask_in_sync(pio0, 1 << smCmdReader | 1 << smDatReader | 1 << smDatWriter);
}

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

/**
 * @brief Interrupt handler called when SEL goes high
 * Notifies main thread to reset SMs and sim thread
 */
void pio0_irq0() {
	// NOTE: This will not block core 1
	// Reset the state machines and sim thread, transaction has ended
	restart_pio_sm();
	pio_interrupt_clear(pio0, 0);
}

void cancel_ack() {
	pio_sm_exec(pio0, smCmdReader, pio_encode_jmp(offsetCmdReader));		// restart smCmdReader
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
					write_byte_blocking(pio0, smDatWriter, mc.flag_byte);
					next_state = MC_COMMAND;
					break;
				case PAD_TOP:
					next_state = PAD_ACCESS;
					// fall through and cancel ack
				default:
					cancel_ack();
			}
			break;
		case PAD_ACCESS:	/* during PAD interactiona always cancel ACKs to avoid interfering */
			cancel_ack();
			
			switch(data) {
				case PAD_READ:
					next_state = PAD_SNIFF;
					break;
				default:
					next_state = MC_IDLE;
			}
			
			break;
		case PAD_SNIFF:
			cancel_ack();
			switch (sm_byte_counter) {
				case 0:
					pio_sm_clear_fifos(pio0, smDatReader);	// clear out Hi-Z, idlo, and idhi bytes
					break;
				case 1: 
					sw_status = read_byte_blocking(pio0, smDatReader);
					break;
				case 2:
					sw_status = sw_status | (read_byte_blocking(pio0, smDatReader) << 8);
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
				default:
					valid_command = false;
					next_state = MC_IDLE;
			}
			if (valid_command) {
				valid_command = false;
				next_state = MC_SEND_ID;
				write_byte_blocking(pio0, smDatWriter, MC_ID1);
			}
			break;
		case MC_SEND_ID:
			if (command_state == MC_EXECUTE_ID) {
				// ID doesn't need to receive an address
				next_state = command_state;
			} else {
				next_state = MC_RECV_ADDR;
			}
			write_byte_blocking(pio0, smDatWriter, MC_ID2);
			break;
		case MC_RECV_ADDR: // receive the address
			if (sm_byte_counter == 0) {
				// Filler
				write_byte_blocking(pio0, smDatWriter, 0x00);
				sm_byte_counter++;
			} else if (sm_byte_counter == 1) {
				// MSB
				sm_address = data << 8;
				// Send MSB
				write_byte_blocking(pio0, smDatWriter, data);
				sm_byte_counter++;
			} else if (sm_byte_counter == 2) {
				// LSB
				sm_address |= data;
				if(command_state == MC_EXECUTE_READ) {
					write_byte_blocking(pio0, smDatWriter, MC_ACK1);
				} else {
					// Otherwise send LSB
					write_byte_blocking(pio0, smDatWriter, data);
				}

				next_state = command_state;
				command_state = MC_IDLE;
				sm_byte_counter = 0;
			}
			break;
		case MC_EXECUTE_ID: // send mc id - used to identify which type of device this is
			if(sm_byte_counter < sizeof(id_data)) {
				write_byte_blocking(pio0, smDatWriter, id_data[sm_byte_counter++]);
			} else {
				next_state = MC_IDLE;
			}
			break;
		case MC_EXECUTE_READ: // do a read operation
			if(sm_byte_counter == 0) {
				// Send ACK2
				write_byte_blocking(pio0, smDatWriter, MC_ACK2);
				checksum = ((sm_address & 0xFF00) >> 8) ^ (sm_address & 0x00FF);
			} else if (sm_byte_counter > 0 && sm_byte_counter < 3) {
				if(memory_card_is_sector_valid(&mc, sm_address)) {
					if (sm_byte_counter == 1) {
						// MSB
						write_byte_blocking(pio0, smDatWriter, (sm_address & 0xFF00) >> 8);
					} else {
						// LSB
						write_byte_blocking(pio0, smDatWriter, (sm_address & 0x00FF));
					}
				} else {
					// Abort transaction - invalid sector
					write_byte_blocking(pio0, smDatWriter, 0xff);
					next_state = MC_ABORT;
				}
			} else {
				// Performing read
				// byte counter is 3 at start here
				uint8_t* sec_ptr = memory_card_get_sector_ptr(&mc, sm_address);
				if ((sm_byte_counter - 3) < MC_SEC_SIZE) {
					write_byte_blocking(pio0, smDatWriter, sec_ptr[sm_byte_counter - 3]);
					checksum ^= sec_ptr[sm_byte_counter - 3];
				} else {
					// Send checksum
					write_byte_blocking(pio0, smDatWriter, checksum);
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
					write_byte_blocking(pio0, smDatWriter, data);
				} else {
					if (sm_byte_counter == MC_SEC_SIZE) {
						// Read checksum
						recv_checksum = data;
						write_byte_blocking(pio0, smDatWriter, MC_ACK1);
					} else {
						// ACK 2
						write_byte_blocking(pio0, smDatWriter, MC_ACK2);
						memory_card_reset_seen_flag(&mc);
						if(sm_address != MC_TEST_SEC) {
							queue_add_blocking(&mc_sector_sync_queue, &sm_address);
						}
						next_state = MC_END;
					}
				}
			} else {
				write_byte_blocking(pio0, smDatWriter, 0xff);
				next_state = MC_ABORT;
			}
			sm_byte_counter++;
			break;
		case MC_ABORT: // something went wrong, abort
			write_byte_blocking(pio0, smDatWriter, 0xff);
			next_state = MC_IDLE;
			break;
		case MC_END: // end
			// Send end byte and update timestamp
			if(recv_checksum == checksum) {
				write_byte_blocking(pio0, smDatWriter, MC_GOOD);
			} else {
				write_byte_blocking(pio0, smDatWriter, MC_BAD_CHK);
			}
			next_state = MC_IDLE;
			break;
		default:
			next_state = MC_IDLE;
			write_byte_blocking(pio0, smDatWriter, 0xff);
	}
}

_Noreturn void simulation_thread() {
	printf("Simulation core begin...\n");
	while(true) {
		mutex_enter_blocking(&mutex_sm_tick);
		uint8_t item = read_byte_blocking(pio0, smCmdReader);
		state_machine_tick(item);
		mutex_exit(&mutex_sm_tick);
	}
}

bool is_mc_switch_safe() {
	return (current_state != MC_EXECUTE_WRITE && next_state != MC_EXECUTE_WRITE && queue_is_empty(&mc_sector_sync_queue));
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
	status = memcard_manager_get_first(mc_file_name);	// get first memory card
	if(status != MM_OK) {
		while(true) {
			led_blink_error(status);
			sleep_ms(1000);
		}
	}
	status = memory_card_import(&mc, mc_file_name);
	if(status != MC_OK) {
		while(true) {
			led_blink_error(status);
			sleep_ms(2000);
		}
	}

	printf("\n\nInitializing memory card simulation...\n");

	/* Setup PIO interrupts */
	irq_set_exclusive_handler(PIO0_IRQ_0, pio0_irq0); // installed on the current core (0)
	irq_set_enabled(PIO0_IRQ_0, true);

	offsetSelMonitor = pio_add_program(pio0, &sel_monitor_program);
	offsetCmdReader = pio_add_program(pio0, &cmd_reader_program);
	offsetDatReader = pio_add_program(pio0, &dat_reader_program);
	offsetDatWriter = pio_add_program(pio0, &dat_writer_program);

	smSelMonitor = pio_claim_unused_sm(pio0, true);
	smCmdReader = pio_claim_unused_sm(pio0, true);
	smDatReader = pio_claim_unused_sm(pio0, true);
	smDatWriter = pio_claim_unused_sm(pio0, true);

	dat_writer_program_init(pio0, smDatWriter, offsetDatWriter, PIN_DAT, PIN_SEL);
	cmd_reader_program_init(pio0, smCmdReader, offsetCmdReader, PIN_CMD, PIN_ACK);
	dat_reader_program_init(pio0, smDatReader, offsetDatReader, PIN_DAT);
	sel_monitor_program_init(pio0, smSelMonitor, offsetSelMonitor, PIN_SEL);


	/* Enable all SM simultaneously */
	uint32_t smMask = (1 << smSelMonitor) | (1 << smCmdReader) | (1 << smDatReader) | (1 << smDatWriter);
	pio_enable_sm_mask_in_sync(pio0, smMask);

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
				uint32_t status;
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
						if(is_mc_switch_safe) {	// and also after
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
				if(is_mc_switch_safe) {	// and also after
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