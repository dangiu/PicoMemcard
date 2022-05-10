#include "memcard_simulator.h"
#include "stdio.h"
#include "pico/stdlib.h"
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

#define IDLE_TIMEOUT_BEFORE_SYNC 1000	// time (in ms) the memory card must be inactive before performing sync to filesystem

PIO pio = pio0;

uint smSelMonitor;
uint smCmdReader;
uint smAckSender;
uint smDatWriter;

uint offsetSelMonitor;
uint offsetCmdReader;
uint offsetDatWriter;
uint offsetAckSender;

/**
 * @brief Interrupt handler called when SEL goes high
 * Resets cmd_reader and dat_wruter state machines
 */
void pio0_irq0() {
	pio_set_sm_mask_enabled(pio, 1 << smCmdReader | 1 << smDatWriter, false);
	pio_restart_sm_mask(pio, 1 << smCmdReader | 1 << smDatWriter);
	pio_sm_exec(pio, smCmdReader, pio_encode_jmp(offsetCmdReader));	// restart smCmdReader PC
	pio_sm_exec(pio, smDatWriter, pio_encode_jmp(offsetDatWriter));	// restart smDatWriter PC
	pio_sm_clear_fifos(pio, smCmdReader);
	pio_sm_clear_fifos(pio, smDatWriter);
	pio_interrupt_clear(pio0, 0);
	pio_enable_sm_mask_in_sync(pio, 1 << smCmdReader | 1 << smDatWriter);
}

void cancel_ack() {
	pio_sm_exec(pio, smAckSender, pio_encode_jmp(offsetAckSender));		// restart smAckSender
}

void process_memcard_read(MemoryCard* mc) {
	uint8_t temp;
	write_byte_blocking(pio, smDatWriter, 0x00);	// send filler
	/* read sector number */
	uint8_t sec_msb = read_byte_blocking(pio, smCmdReader);
	
	write_byte_blocking(pio, smDatWriter, sec_msb);	// send msb
	uint8_t sec_lsb = read_byte_blocking(pio, smCmdReader);

	/* queue acks */
	write_byte_blocking(pio, smDatWriter, MC_ACK1);
	temp = read_byte_blocking(pio, smCmdReader);
	if(temp != 0x00) {
		return;
	}

	write_byte_blocking(pio, smDatWriter, MC_ACK2);
	temp = read_byte_blocking(pio, smCmdReader);
	if(temp != 0x00) {
		return;
	}
	
	/* queue sector confirm */
	if(memory_card_is_sector_valid(mc, (uint32_t) sec_msb << 8 | sec_lsb)) {
		write_byte_blocking(pio, smDatWriter, sec_msb);
		temp = read_byte_blocking(pio, smCmdReader);
		if(temp != 0x00) {
			return;
		}
		write_byte_blocking(pio, smDatWriter, sec_lsb);
		temp = read_byte_blocking(pio, smCmdReader);
		if(temp != 0x00) {
			return;
		}
	} else {
		/* invalid sector, queue 0xff 0xff and abort transfer */
		write_byte_blocking(pio, smDatWriter, 0xff);
		temp = read_byte_blocking(pio, smCmdReader);	// discard cmd
		write_byte_blocking(pio, smDatWriter, 0xff);
		temp = read_byte_blocking(pio, smCmdReader);	// discard cmd
		return;
	}

	/* queue sector data while performing xor */
	uint8_t* sec_ptr = memory_card_get_sector_ptr(mc, (uint32_t) sec_msb << 8 | sec_lsb);
	
	uint8_t checksum = sec_msb ^ sec_lsb;
	for(uint32_t i = 0; i < MC_SEC_SIZE; ++i) {
		write_byte_blocking(pio, smDatWriter, sec_ptr[i]);
		temp = read_byte_blocking(pio, smCmdReader);
		if(temp != 0x00) {
			return;
		}

		checksum = checksum ^ sec_ptr[i];
	}
	
	/* queue checksum */
	write_byte_blocking(pio, smDatWriter, checksum);
	temp = read_byte_blocking(pio, smCmdReader);
	if(temp != 0x00) {
		return;
	}

	/* queue end byte */
	write_byte_blocking(pio, smDatWriter, MC_GOOD);
	temp = read_byte_blocking(pio, smCmdReader);
	cancel_ack();	// end-of-protocol, don't need to send more data
	if(temp != 0x00) {
		return;
	}

	memory_card_update_timestamp(mc);
	printf("READ  %.2x%.2x\n", sec_msb, sec_lsb);
	return;
}

void process_memcard_write(MemoryCard* mc) {
	uint8_t temp;
	write_byte_blocking(pio, smDatWriter, 0x00);	// send filler
	
	/* read sector number */
	uint8_t sec_msb = read_byte_blocking(pio, smCmdReader);
	
	write_byte_blocking(pio, smDatWriter, sec_msb);	// send msb
	uint8_t sec_lsb = read_byte_blocking(pio, smCmdReader);

	write_byte_blocking(pio, smDatWriter, sec_lsb);	// queue lsb

	/* receive sector data while performing xor and queueing pre */
	uint8_t* sec_ptr = memory_card_get_sector_ptr(mc, (uint32_t) sec_msb << 8 | sec_lsb);

	uint8_t pre = sec_lsb;
	uint8_t checksum = sec_msb ^ sec_lsb;
	for(uint32_t i = 0; i < MC_SEC_SIZE; ++i) {
		temp = read_byte_blocking(pio, smCmdReader);
		write_byte_blocking(pio, smDatWriter, temp);
		checksum = checksum ^ temp;

		if(memory_card_is_sector_valid(mc, (uint32_t) sec_msb << 8 | sec_lsb)) {	// save sector data only if sector address is valid
			sec_ptr[i] = temp;
		}
	}

	/* read checksum */
	uint8_t chk = read_byte_blocking(pio, smCmdReader);

	/* queue acks */
	write_byte_blocking(pio, smDatWriter, MC_ACK1);
	temp = read_byte_blocking(pio, smCmdReader);
	if(temp != 0x00) {
		return;
	}

	write_byte_blocking(pio, smDatWriter, MC_ACK2);
	temp = read_byte_blocking(pio, smCmdReader);
	if(temp != 0x00) {
		return;
	}

	/* queue end byte */
	if(!memory_card_is_sector_valid(mc, (uint32_t) sec_msb << 8 | sec_lsb)) {	// invalid sector, write not performed
		write_byte_blocking(pio, smDatWriter, MC_BAD_SEC);
	} else if(chk != checksum) {	// bad checksum, write might be corrupted
		write_byte_blocking(pio, smDatWriter, MC_BAD_CHK);
	} else {	// write performed, no errors
		write_byte_blocking(pio, smDatWriter, MC_GOOD);
	}
	temp = read_byte_blocking(pio, smCmdReader);
	cancel_ack();	// end-of-protocol, don't need to send more data
	if(temp != 0x00) {
		return;
	}

	memory_card_reset_seen_flag(mc);	// when first write is performed, reset the "is-new" flag of memory card
	if((uint32_t) sec_msb << 8 | sec_lsb != MC_TEST_SEC) {
		memory_card_set_sync(mc, true);
	}
	memory_card_update_timestamp(mc);
	printf("WRITE  %.2x%.2x\n", sec_msb, sec_lsb);
	return;
}


void process_memcard_id(MemoryCard* mc) {
	//(void) mc;
	uint8_t temp;
	
	/* queue acks */
	write_byte_blocking(pio, smDatWriter, MC_ACK1);
	temp = read_byte_blocking(pio, smCmdReader);
	if(temp != 0x00) {
		return;
	}

	write_byte_blocking(pio, smDatWriter, MC_ACK2);
	temp = read_byte_blocking(pio, smCmdReader);
	if(temp != 0x00) {
		return;
	}

	/* queue ID sequence */
	write_byte_blocking(pio, smDatWriter, 0x04);
	temp = read_byte_blocking(pio, smCmdReader);
	if(temp != 0x00) {
		return;
	}

	write_byte_blocking(pio, smDatWriter, 0x00);
	temp = read_byte_blocking(pio, smCmdReader);
	if(temp != 0x00) {
		return;
	}

	write_byte_blocking(pio, smDatWriter, 0x00);
	temp = read_byte_blocking(pio, smCmdReader);
	if(temp != 0x00) {
		return;
	}

	write_byte_blocking(pio, smDatWriter, 0x80);
	temp = read_byte_blocking(pio, smCmdReader);
	if(temp != 0x00) {
		return;
	}
	cancel_ack();	// end-of-protocol, don't need to send more data

	memory_card_update_timestamp(mc);
	printf("ID\n");
	return;
}

void process_memcard_req(MemoryCard* mc) {
	bool error = false;
	write_byte_blocking(pio, smDatWriter, mc->flag_byte);	// queue MC flag
	uint8_t memcard_cmd = read_byte_blocking(pio, smCmdReader);
	
	/* queue MC ID bytes */
	write_byte_blocking(pio, smDatWriter, MC_ID1);
	uint8_t fill1 = read_byte_blocking(pio, smCmdReader);

	write_byte_blocking(pio, smDatWriter, MC_ID2);
	uint8_t fill2 = read_byte_blocking(pio, smCmdReader);

	if(fill1 != 0x00 || fill2 != 0x00) {	// filler is wrong abort protocol
		return;
	}

	switch(memcard_cmd) {
		case MEMCARD_READ:
			process_memcard_read(mc);
			break;
		case MEMCARD_WRITE:
			process_memcard_write(mc);
			break;
		case MEMCARD_ID:
			process_memcard_id(mc);
			break;
		default:
			printf("\nUnknown MC CMD: %.2x\n", memcard_cmd);
			break;
	}

	return;
}

int simulate_memory_card() {
	MemoryCard mc;

	memory_card_init(&mc);

	printf("\n\nInitializing memory card simulation...\n");

	/* Setup PIO interrupts */
	irq_set_exclusive_handler(PIO0_IRQ_0, pio0_irq0);
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

	while(true) {
		uint8_t item = read_byte_blocking(pio, smCmdReader);
		if(item == MEMCARD_TOP) {
			process_memcard_req(&mc);
		} else {
			cancel_ack();
		}
		if(mc.out_of_sync) {
			if(to_ms_since_boot(get_absolute_time()) - mc.last_operation_timestamp > IDLE_TIMEOUT_BEFORE_SYNC){
				printf("SYNC	");
				if(0 == memory_card_sync(&mc)) {
					memory_card_set_sync(&mc, false);
					printf("OK\n");
				} else {
					printf("FAIL\n");
				}
			}
		}
	}
}