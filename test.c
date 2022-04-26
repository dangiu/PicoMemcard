/**
 * @file test.c
 * @author Daniele Giuliani (danielegiuliani0@gmail.com)
 * @brief Testing interaction of PIO and PSX by simulating Joypad
 * @version 0.1
 * @date 2022-04-08
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/irq.h"
#include "psxSPI.pio.h"

#define PIN_DAT 5
#define PIN_CMD 6
#define PIN_SEL 7
#define PIN_CLK 8
#define PIN_ACK 9

#define PIN_LED 25

#define ID_LO 0x41
#define ID_HI 0x5A

#define R_PRESSED 0b11011111
#define L_PRESSED 0b01111111
#define NO_PRESSED 0xff
#define SEND_ACK 0b100000000


PIO pio = pio0;

uint smSelMonitor;
uint smCmdReader;
uint smAckSender;
uint smDatWriter;

uint offsetSelMonitor;
uint offsetCmdReader;
uint offsetDatWriter;
uint offsetAckSender;

static uint count = 0;

/**
 * @brief Interrupt handler called when SEL goes high
 * Resets cmdReader and datWriter state machines
 */
void pio0_irq0() {
	pio_set_sm_mask_enabled(pio, 1 << smCmdReader | 1 << smDatWriter, false);
	pio_restart_sm_mask(pio, 1 << smCmdReader | 1 << smDatWriter);
	pio_sm_exec(pio, smCmdReader, pio_encode_jmp(offsetCmdReader));	// restart smCmdReader PC
	pio_sm_exec(pio, smDatWriter, pio_encode_jmp(offsetDatWriter));	// restart smDatWriter PC
	pio_enable_sm_mask_in_sync(pio, 1 << smCmdReader | 1 << smDatWriter);
	pio_interrupt_clear(pio0, 0);
}

void cancel_ack() {
	//pio_sm_exec(pio, smAckSender, pio_encode_jmp(offsetCmdReader));		// restart smCmdReader
}

void process_joy_req(uint8_t next_byte) {
	printf("\n%.2x ", next_byte);
	write_dat_LSB_blocking(pio, smDatWriter, ID_LO);	// write lower ID byte

	next_byte = read_cmd_byte_blocking(pio, smCmdReader);
	if(next_byte != 0x42) {
		printf("\nWaiting for %.2x but received %.2x\n", 0x42, next_byte);
		return;
	} else {
		write_dat_LSB_blocking(pio, smDatWriter, ID_HI);	// write upper ID byte
		printf("%.2x ", next_byte);
	}

	next_byte = read_cmd_byte_blocking(pio, smCmdReader);
	if(next_byte != 0x00) {
		printf("\nWaiting for %.2x but received %.2x\n", 0x00, next_byte);
		return;
	} else {
		switch (count) {
		case 0:
			write_dat_LSB_blocking(pio, smDatWriter, R_PRESSED);	// fake right d-pad being pressed
			break;
		case 2:
			write_dat_LSB_blocking(pio, smDatWriter, L_PRESSED);	// fake left d-pad being pressed
			break;
		case 1:
		case 3:		
		default:
			write_dat_LSB_blocking(pio, smDatWriter, NO_PRESSED);	// release all buttons
			break;
		}
		printf("%.2x ", next_byte);
	}

	next_byte = read_cmd_byte_blocking(pio, smCmdReader);
	if(next_byte != 0x00) {
		printf("\nWaiting for %.2x but received %.2x\n", 0x00, next_byte);
		return;
	} else {
		write_dat_LSB_blocking(pio, smDatWriter, NO_PRESSED);	// other buttons are all inactive
		printf("%.2x ", next_byte);
	}

	next_byte = read_cmd_byte_blocking(pio, smCmdReader);
	cancel_ack();	// last byte being received, no need to send more data
	if(next_byte != 0x00) {
		printf("\nWaiting for %.2x but received %.2x\n", 0x00, next_byte);
		return;
	} else {
		printf("%.2x ", next_byte);
	}
}

int main() {
	stdio_init_all();

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

	uint32_t i = 0;
	while(true) {
		uint8_t item = read_cmd_byte_blocking(pio, smCmdReader);
		
		if(item == 0x01)
			process_joy_req(item);
			count = (count + 1) % 4;

	}
}