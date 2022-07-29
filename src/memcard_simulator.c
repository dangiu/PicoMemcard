#include "memcard_simulator.h"
#include "stdio.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/pio.h"
#include "hardware/irq.h"
#include "psxSPI.pio.h"
#include "memory_card.h"
#include "led.h"
#include "config.h"

#define MEMCARD_TOP 0x81
#define MEMCARD_READ 0x52
#define MEMCARD_WRITE 0x57
#define MEMCARD_ID 0x53

PIO pio = pio0;

uint smSelMonitor;
uint smCmdReader;
uint smAckSender;
uint smDatWriter;

uint offsetSelMonitor;
uint offsetCmdReader;
uint offsetDatWriter;
uint offsetAckSender;

memory_card_t mc;

enum memcard_states {
	MC_IDLE = 1,
	MC_COMMAND,
	MC_SEND_ID,
	MC_RECV_ADDR,
	MC_EXECUTE_READ,
	MC_EXECUTE_WRITE,
	MC_EXECUTE_ID,
	MC_ABORT,
	MC_END
};

uint8_t current_state = MC_IDLE;
uint8_t next_state = MC_IDLE;
uint8_t command_state = MC_IDLE;
uint8_t checksum = 0x00;
uint8_t recv_checksum = 0x00;
uint8_t sm_byte_counter = 0;
uint16_t sm_address = 0x0000;
uint8_t id_data[] = {MC_ACK1, MC_ACK2, 0x04, 0x00, 0x00, 0x80};

_Noreturn void simulation_thread();

/**
 * @brief Interrupt handler called when SEL goes high
 * Notifies main thread to reset SMs and sim thread
 */
void pio0_irq0() {
	// NOTE: This will not block core 1
	// Reset the state machines and sim thread, transaction has ended
	pio_set_sm_mask_enabled(pio, 1 << smCmdReader | 1 << smDatWriter | 1 << smAckSender, false);
	pio_restart_sm_mask(pio, 1 << smCmdReader | 1 << smDatWriter | 1 << smAckSender);
	pio_sm_exec(pio, smCmdReader, pio_encode_jmp(offsetCmdReader));	// restart smCmdReader PC
	pio_sm_exec(pio, smDatWriter, pio_encode_jmp(offsetDatWriter));	// restart smDatWriter PC
	pio_sm_exec(pio, smAckSender, pio_encode_jmp(offsetAckSender));	// restart smAckSender PC
	pio_sm_clear_fifos(pio, smCmdReader);
	pio_sm_drain_tx_fifo(pio, smDatWriter); // drain instead of clear, so that we empty the OSR

	// Reset mc state machine
	current_state = MC_IDLE;
	next_state = MC_IDLE;
	command_state = MC_IDLE;
	sm_byte_counter = 0;
	sm_address = 0x0000;
	checksum = 0x00;
	recv_checksum = 0x00;

	pio_enable_sm_mask_in_sync(pio, 1 << smCmdReader | 1 << smDatWriter | 1 << smAckSender);
	pio_interrupt_clear(pio0, 0);
}

void cancel_ack() {
	pio_sm_exec(pio, smAckSender, pio_encode_jmp(offsetAckSender));		// restart smAckSender
}

bool should_sync() {
	if(current_state == MC_IDLE) {
		if(mc.out_of_sync) {
			if((to_ms_since_boot(get_absolute_time()) - mc.last_operation_timestamp) > IDLE_AUTOSYNC_TIMEOUT)
				return true;
		}
	}
	return false;
}

void state_machine_tick(uint8_t data) {
	bool valid_command = false;
	current_state = next_state;

	switch(current_state) {
		case MC_IDLE: // idle / sleeping
			sm_byte_counter = 0;
			command_state = MC_IDLE;
			checksum = 0x00;
			recv_checksum = 0x00;
			sm_address = 0x0000;
			next_state = MC_IDLE;
			if (data == MEMCARD_TOP) {
				// Send flag byte and start transaction
				write_byte_blocking(pio0, smDatWriter, mc.flag_byte);
				next_state = MC_COMMAND;
			} else {
				cancel_ack();
			}
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
					break;
			}
			if (valid_command) {
				valid_command = false;
				next_state = MC_SEND_ID;
				write_byte_blocking(pio, smDatWriter, MC_ID1);
			}
			break;
		case MC_SEND_ID:
			if (command_state == MC_EXECUTE_ID) {
				// ID doesn't need to receive an address
				next_state = command_state;
			} else {
				next_state = MC_RECV_ADDR;
			}
			write_byte_blocking(pio, smDatWriter, MC_ID2);
			break;
		case MC_RECV_ADDR: // receive the address
			if (sm_byte_counter == 0) {
				// Filler
				write_byte_blocking(pio, smDatWriter, 0x00);
				sm_byte_counter++;
			} else if (sm_byte_counter == 1) {
				// MSB
				sm_address = data << 8;
				// Send MSB
				write_byte_blocking(pio, smDatWriter, data);
				sm_byte_counter++;
			} else if (sm_byte_counter == 2) {
				// LSB
				sm_address |= data;
				if(command_state == MC_EXECUTE_READ) {
					write_byte_blocking(pio, smDatWriter, MC_ACK1);
				} else {
					// Otherwise send LSB
					write_byte_blocking(pio, smDatWriter, data);
				}

				next_state = command_state;
				command_state = MC_IDLE;
				sm_byte_counter = 0;
			}
			break;
		case MC_EXECUTE_ID: // send mc id - used to identify which type of device this is
			if(sm_byte_counter < sizeof(id_data)) {
				write_byte_blocking(pio, smDatWriter, id_data[sm_byte_counter++]);
			} else {
				next_state = MC_IDLE;
			}
			break;
		case MC_EXECUTE_READ: // do a read operation
			if(sm_byte_counter == 0) {
				// Send ACK2
				write_byte_blocking(pio, smDatWriter, MC_ACK2);
				checksum = ((sm_address & 0xFF00) >> 8) ^ (sm_address & 0x00FF);
			} else if (sm_byte_counter > 0 && sm_byte_counter < 3) {
				if(memory_card_is_sector_valid(&mc, sm_address)) {
					if (sm_byte_counter == 1) {
						// MSB
						write_byte_blocking(pio, smDatWriter, (sm_address & 0xFF00) >> 8);
					} else {
						// LSB
						write_byte_blocking(pio, smDatWriter, (sm_address & 0x00FF));
					}
				} else {
					// Abort transaction - invalid sector
					write_byte_blocking(pio, smDatWriter, 0xff);
					next_state = MC_ABORT;
				}
			} else {
				// Performing read
				// byte counter is 3 at start here
				uint8_t* sec_ptr = memory_card_get_sector_ptr(&mc, sm_address);
				if ((sm_byte_counter - 3) < MC_SEC_SIZE) {
					write_byte_blocking(pio, smDatWriter, sec_ptr[sm_byte_counter - 3]);
					checksum ^= sec_ptr[sm_byte_counter - 3];
				} else {
					// Send checksum
					write_byte_blocking(pio, smDatWriter, checksum);
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
					write_byte_blocking(pio, smDatWriter, data);
				} else {
					if (sm_byte_counter == MC_SEC_SIZE) {
						// Read checksum
						recv_checksum = data;
						write_byte_blocking(pio, smDatWriter, MC_ACK1);
					} else {
						// ACK 2
						write_byte_blocking(pio, smDatWriter, MC_ACK2);
						memory_card_reset_seen_flag(&mc);
						if(sm_address != MC_TEST_SEC) {
							memory_card_set_sync(&mc, true);
						}
						next_state = MC_END;
					}
				}
			} else {
				write_byte_blocking(pio, smDatWriter, 0xff);
				next_state = MC_ABORT;
			}
			sm_byte_counter++;
			break;
		case MC_ABORT: // something went wrong, abort
			write_byte_blocking(pio, smDatWriter, 0xff);
			next_state = MC_IDLE;
			break;
		case MC_END: // end
			// Send end byte and update timestamp
			if(recv_checksum == checksum) {
				write_byte_blocking(pio, smDatWriter, MC_GOOD);
			} else {
				write_byte_blocking(pio, smDatWriter, MC_BAD_CHK);
			}
			memory_card_update_timestamp(&mc);
			next_state = MC_IDLE;
			break;
		default:
			next_state = MC_IDLE;
			write_byte_blocking(pio, smDatWriter, 0xff);
	}
}

_Noreturn void simulation_thread() {
	while(true) {
		uint8_t item = read_byte_blocking(pio, smCmdReader);
		state_machine_tick(item);
		if(should_sync()) {
			memory_card_sync(&mc);
		}
	}
}

_Noreturn int simulate_memory_card() {
	uint32_t status;
	status = memory_card_init(&mc);
	if(status != MC_OK) {
		while(true) {
			blink_normal(status);
			sleep_ms(2000);
		}
	}
	status = memory_card_import(&mc, MEMCARD_FILE_NAME);
	if(status != MC_OK) {
		while(true) {
			blink_normal(status);
			sleep_ms(2000);
		}
	}

	printf("\n\nInitializing memory card simulation...\n");

	/* Setup PIO interrupts */
	irq_set_exclusive_handler(PIO0_IRQ_0, pio0_irq0); // installed on the current core (0)
	irq_set_enabled(PIO0_IRQ_0, true);

	offsetSelMonitor = pio_add_program(pio, &sel_monitor_program);
	offsetCmdReader = pio_add_program(pio, &cmd_reader_program);
	offsetAckSender = pio_add_program(pio, &ack_sender_program);
	offsetDatWriter = pio_add_program(pio, &dat_writer_program);

	smSelMonitor = pio_claim_unused_sm(pio, true);
	smCmdReader = pio_claim_unused_sm(pio, true);
	smAckSender = pio_claim_unused_sm(pio, true);
	smDatWriter = pio_claim_unused_sm(pio, true);

	dat_writer_program_init(pio, smDatWriter, offsetDatWriter, PIN_DAT, PIN_CLK);
	ack_sender_program_init(pio, smAckSender, offsetAckSender, PIN_ACK);
	cmd_reader_program_init(pio, smCmdReader, offsetCmdReader, PIN_CMD);
	sel_monitor_program_init(pio, smSelMonitor, offsetSelMonitor, PIN_SEL);


	/* Enable all SM simultaneously */
	uint32_t smMask = (1 << smSelMonitor) | (1 << smCmdReader) | (1 << smAckSender) | (1 << smDatWriter);
	pio_enable_sm_mask_in_sync(pio, smMask);

	/* Launch memory card thread */
	printf("\n\nStarting sim thread...\n");
	multicore_launch_core1(simulation_thread);
	multicore_lockout_victim_init();	// prepare core0 to be locked by core1 (when writing flash)

	while(true) {
		// do nothing, now the core is only responsible for managing interrupt (when SEL goes high)
	}
}