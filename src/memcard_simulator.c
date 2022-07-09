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

#define PIN_DAT 5
#define PIN_CMD 6
#define PIN_SEL 7
#define PIN_CLK 8
#define PIN_ACK 9

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

MemoryCard *mc;
queue_t mc_data_sync_queue;
critical_section_t sync_write_section;

typedef struct {
	uint16_t address;
	uint8_t data[MC_SEC_SIZE];
} mc_sync_entry_t;

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

mc_sync_entry_t sync_entry;

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
    sync_entry.address = 0x0000;
    memset(&sync_entry.data, 0x00, 128);

	pio_enable_sm_mask_in_sync(pio, 1 << smCmdReader | 1 << smDatWriter | 1 << smAckSender);
	pio_interrupt_clear(pio0, 0);
}

void cancel_ack() {
	pio_sm_exec(pio, smAckSender, pio_encode_jmp(offsetAckSender));		// restart smAckSender
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
                write_byte_blocking(pio0, smDatWriter, mc->flag_byte);
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
                sync_entry.address = sm_address;
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
                if(memory_card_is_sector_valid(mc, sm_address)) {
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
                uint8_t* sec_ptr = memory_card_get_sector_ptr(mc, sm_address);
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
            if(memory_card_is_sector_valid(mc, sm_address)) {
                uint8_t* sec_ptr = memory_card_get_sector_ptr(mc, sm_address);
                if(sm_byte_counter == 0) {
                    checksum = ((sm_address & 0xFF00) >> 8) ^ (sm_address & 0x00FF);
                }
                if(sm_byte_counter < MC_SEC_SIZE) {
                    checksum ^= data;
                    sec_ptr[sm_byte_counter] = data;
                    sync_entry.data[sm_byte_counter] = data;
                    write_byte_blocking(pio, smDatWriter, data);
                } else {
                    if (sm_byte_counter == MC_SEC_SIZE) {
                        // Read checksum
                        recv_checksum = data;
                        write_byte_blocking(pio, smDatWriter, MC_ACK1);
                    } else {
                        // ACK 2
                        write_byte_blocking(pio, smDatWriter, MC_ACK2);
                        memory_card_reset_seen_flag(mc);
                        if(sm_address != MC_TEST_SEC) {
                            queue_add_blocking(&mc_data_sync_queue, &sync_entry);
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
	}
}

_Noreturn int simulate_memory_card() {
	/* We need to allocate the memory for the memcard struct here, or else we'll run out of memory in USB mode */
	mc = (MemoryCard*)malloc(sizeof(MemoryCard));
	memset((void*)mc, 0x00, sizeof(MemoryCard));

	queue_init(&mc_data_sync_queue, sizeof(mc_sync_entry_t), 4);

    critical_section_init(&sync_write_section);

	memory_card_init(mc);

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

	while(true) {
		if(!queue_is_empty(&mc_data_sync_queue)) {
			mc_sync_entry_t next_entry;
			queue_remove_blocking(&mc_data_sync_queue, &next_entry);
            printf("ADDR 0x%X\n", next_entry.address);
			memory_card_sync_page(mc, next_entry.address, next_entry.data);
		}
	}
}