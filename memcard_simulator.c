/**
 * @file memcard_simulator.c
 * @author Daniele Giuliani (danielegiuliani0@gmail.com)
 * @brief Simulating a PSX Memory Card using a PIO
 * @version 0.1
 * @date 2022-04-08
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/irq.h"
#include "psxSPI.pio.h"
#include "memory_card.h"
#include "mc_raw.h"

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

MemoryCard mc;

/**
 * @brief Interrupt handler called when SEL goes high
 * Resets cmdReader and datWriter state machines
 */
void pio0_irq0() {
	pio_set_sm_mask_enabled(pio, 1 << smCmdReader | 1 << smDatWriter, false);
	pio_restart_sm_mask(pio, 1 << smCmdReader | 1 << smDatWriter);
	pio_sm_exec(pio, smCmdReader, pio_encode_jmp(offsetCmdReader));	// restart smCmdReader PC
	pio_sm_exec(pio, smDatWriter, pio_encode_jmp(offsetDatWriter));	// restart smDatWriter PC
	cancel_read = true;
	pio_interrupt_clear(pio0, 0);
	pio_enable_sm_mask_in_sync(pio, 1 << smCmdReader | 1 << smDatWriter);
}

void suspend_proto() {
	pio_set_sm_mask_enabled(pio, 1 << smCmdReader | 1 << smDatWriter | 1 << smAckSender, false);
	pio_restart_sm_mask(pio, 1 << smCmdReader | 1 << smDatWriter | 1 << smAckSender);
}

void restart_proto() {
	pio_sm_clear_fifos(pio, smCmdReader);
	pio_sm_clear_fifos(pio, smDatWriter);
	pio_sm_exec(pio, smCmdReader, pio_encode_jmp(offsetCmdReader));	// restart smCmdReader PC
	pio_sm_exec(pio, smDatWriter, pio_encode_jmp(offsetDatWriter));	// restart smDatWriter PC
	pio_sm_exec(pio, smAckSender, pio_encode_jmp(offsetAckSender));	// restart smAckSender
	pio_enable_sm_mask_in_sync(pio, 1 << smCmdReader | 1 << smDatWriter | 1 << smAckSender);
}

void cancel_ack() {
	pio_sm_exec(pio, smAckSender, pio_encode_jmp(offsetAckSender));		// restart smAckSender
}

void process_memcard_read() {
	uint8_t temp;
	write_dat_LSB_blocking(pio, smDatWriter, 0x00);	// send filler
	/* read sector number */
	uint8_t sec_msb = read_cmd_byte_blocking(pio, smCmdReader);
	
	write_dat_LSB_blocking(pio, smDatWriter, sec_msb);	// send msb
	uint8_t sec_lsb = read_cmd_byte_blocking(pio, smCmdReader);

	/* queue acks */
	write_dat_LSB_blocking(pio, smDatWriter, MC_ACK1);
	temp = read_cmd_byte_blocking(pio, smCmdReader);

	if(temp != 0x00) {
		suspend_proto();
		printf("\n BAD ACK1 FILL %.2x\n", temp);
		printf("\n MSB %.2x   LSB %.2x\n", sec_msb, sec_lsb);
		restart_proto();
		return;
	}
	write_dat_LSB_blocking(pio, smDatWriter, MC_ACK2);
	temp = read_cmd_byte_blocking(pio, smCmdReader);

	if(temp != 0x00) {
		suspend_proto();
		printf("\n BAD ACK2 FILL %.2x\n", temp);
		printf("\n MSB %.2x   LSB %.2x\n", sec_msb, sec_lsb);
		restart_proto();
		return;
	}
	
	/* queue sector confirm */
	if(memory_card_is_sector_valid((uint32_t) sec_msb << 8 | sec_lsb)) {
		write_dat_LSB_blocking(pio, smDatWriter, sec_msb);
		temp = read_cmd_byte_blocking(pio, smCmdReader);

		if(temp != 0x00) {
			suspend_proto();
			printf("\n BAD MSB CONF FILL %.2x\n", temp);
			restart_proto();
			return;
		}
		write_dat_LSB_blocking(pio, smDatWriter, sec_lsb);
		temp = read_cmd_byte_blocking(pio, smCmdReader);

		if(temp != 0x00) {
			suspend_proto();
			printf("\n BAD LSB CONF FILL %.2x\n", temp);
			restart_proto();
			return;
		}
	} else {
		/* SEE WHAT TO DO */
		//write_dat_LSB_blocking(pio, smDatWriter, 0xff);
		//write_dat_LSB_blocking(pio, smDatWriter, 0xff);
		// restart protocol
		suspend_proto();
		printf("\nInvalid MC sector: %d\n", (uint32_t) sec_msb << 8 | sec_lsb);
		restart_proto();
		return;
	}

	/* queue sector data while performing xor */
	uint8_t* sec_ptr = memory_card_get_sector_ptr(&mc, (uint32_t) sec_msb << 8 | sec_lsb);
	
	uint8_t checksum = sec_msb ^ sec_lsb;
	for(uint32_t i = 0; i < MC_SEC_SIZE; ++i) {
		write_dat_LSB_blocking(pio, smDatWriter, sec_ptr[i]);
		//write_dat_LSB_blocking(pio, smDatWriter, 0x00);
		temp = read_cmd_byte_blocking(pio, smCmdReader);

		if(temp != 0x00) {
			suspend_proto();
			printf("\n BAD SEC DATA FILL %.2x\n", temp);
			restart_proto();
			return;
		}
		checksum = checksum ^ sec_ptr[i];
		//checksum = checksum ^ 0x00;
	}
	
	/* queue checksum */
	write_dat_LSB_blocking(pio, smDatWriter, checksum);
	temp = read_cmd_byte_blocking(pio, smCmdReader);

	if(temp != 0x00) {
		suspend_proto();
		printf("\n BAD CHKSUM FILL %.2x\n", temp);
		restart_proto();
		return;
	}

	/* queue end byte */
	write_dat_LSB_blocking(pio, smDatWriter, MC_GOOD);
	temp = read_cmd_byte_blocking(pio, smCmdReader);
	cancel_ack();

	if(temp != 0x00) {
		suspend_proto();
		printf("\n BAD END BYTE FILL %.2x\n", temp);
		restart_proto();
		return;
	}

	printf("READ  %.2x%.2x\n", sec_msb, sec_lsb);
	return;
}

void process_memcard_write() {
	uint8_t temp;
	write_dat_LSB_blocking(pio, smDatWriter, 0x00);	// send filler
	/* read sector number */
	uint8_t sec_msb = read_cmd_byte_blocking(pio, smCmdReader);
	
	write_dat_LSB_blocking(pio, smDatWriter, sec_msb);	// send msb
	uint8_t sec_lsb = read_cmd_byte_blocking(pio, smCmdReader);

	write_dat_LSB_blocking(pio, smDatWriter, sec_lsb);	// queue lsb

	/* receive sector data while performing xor and queueing pre */
	uint8_t* sec_ptr = memory_card_get_sector_ptr(&mc, (uint32_t) sec_msb << 8 | sec_lsb);

	uint8_t pre = sec_lsb;
	uint8_t checksum = sec_msb ^ sec_lsb;
	for(uint32_t i = 0; i < MC_SEC_SIZE; ++i) {
		sec_ptr[i] = read_cmd_byte_blocking(pio, smCmdReader);
		write_dat_LSB_blocking(pio, smDatWriter, sec_ptr[i]);
		checksum = checksum ^ sec_ptr[i];
	}

	uint8_t chk = read_cmd_byte_blocking(pio, smCmdReader);

	if(chk != checksum) {
		printf("CHECKSUM Received: %.2x\tCalculated: %.2x", chk, checksum);
		// TODO change and implement the send of the correct answer
	}

	/* queue acks */
	write_dat_LSB_blocking(pio, smDatWriter, MC_ACK1);
	temp = read_cmd_byte_blocking(pio, smCmdReader);

	if(temp != 0x00) {
		suspend_proto();
		printf("\n BAD ACK1 FILL %.2x\n", temp);
		printf("\n MSB %.2x   LSB %.2x\n", sec_msb, sec_lsb);
		restart_proto();
		return;
	}
	write_dat_LSB_blocking(pio, smDatWriter, MC_ACK2);
	temp = read_cmd_byte_blocking(pio, smCmdReader);

	if(temp != 0x00) {
		suspend_proto();
		printf("\n BAD ACK2 FILL %.2x\n", temp);
		printf("\n MSB %.2x   LSB %.2x\n", sec_msb, sec_lsb);
		restart_proto();
		return;
	}

	/* queue end byte */
	write_dat_LSB_blocking(pio, smDatWriter, MC_GOOD);
	temp = read_cmd_byte_blocking(pio, smCmdReader);
	cancel_ack();

	if(temp != 0x00) {
		suspend_proto();
		printf("\n BAD END BYTE FILL %.2x\n", temp);
		restart_proto();
		return;
	}

	memory_card_reset_seen_flag(&mc);
	printf("WRITE  %.2x%.2x\n", sec_msb, sec_lsb);
	return;
}

void process_memcard_req() {
	bool error = false;
	write_dat_LSB_blocking(pio, smDatWriter, mc.flag_byte);	// queue MC flag
	uint8_t memcard_cmd = read_cmd_byte_blocking(pio, smCmdReader);
	/* queue MC ID bytes */
	write_dat_LSB_blocking(pio, smDatWriter, MC_ID1);
	uint8_t d1 = read_cmd_byte_blocking(pio, smCmdReader);	// discard received filler bytes

	write_dat_LSB_blocking(pio, smDatWriter, MC_ID2);
	uint8_t d2 = read_cmd_byte_blocking(pio, smCmdReader);	// discard received filler bytes

	if(d1 != 0x00 || d2 != 0x00) {	// filler is wrong restart protocol (SM)
		return;
	}

	switch(memcard_cmd) {
		case MEMCARD_READ:
			process_memcard_read();
			break;
		case MEMCARD_WRITE:
			process_memcard_write();
			break;
		default:
			printf("\nMC CMD: %.2x\n", memcard_cmd);
			break;
	}

	return;
}

int main() {
	stdio_init_all();

	memory_card_init(&mc, DATA);

	printf("\n\nBeginning Execution...\n");

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
		uint8_t item = read_cmd_byte_blocking(pio, smCmdReader);
		if(item == MEMCARD_TOP) {
			process_memcard_req(item);
		} else {
			cancel_ack();
		}
	}
}