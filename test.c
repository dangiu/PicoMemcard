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

#define ID_LO 0x41
#define ID_HI 0x5A

#define R_PRESSED 0b11111011
#define L_PRESSED 0b11111110
#define NO_PRESSED 0xff
#define SEND_ACK 0b100000000


uint smReadCmd;
uint smSendAck;
uint smWriteDat;


void pio0_irq0() {
	/* SEL has gone high, reset ISR of smReadCmd (could contain garbage) before it goes low again*/
	pio_sm_restart(pio0, smReadCmd);
	irq_clear(PIO0_IRQ_0);	// clear IRQ flag
	pio_interrupt_clear(pio0, 0);
}

void process_joy_req(PIO pio) {
	sendAck(pio);	// acknowledge top command

	uint32_t next_byte = 0;
	sendByte(pio, smWriteDat, ID_LO);	// write lower ID byte

	next_byte = pio_sm_get_blocking(pio, smReadCmd) >> 24;	// shift to get MSB
	if(next_byte != 0x42) {
		printf("\nWaiting for %.2x but received %.2x\n", 0x42, next_byte);
		return;
	} else {
		printf("%.2x ", next_byte);
	}
	sendAck(pio);
	sendByte(pio, smWriteDat, ID_HI);	// write upper ID byte

	next_byte = pio_sm_get_blocking(pio, smReadCmd) >> 24;	// shift to get MSB
	if(next_byte != 0x00) {
		printf("\nWaiting for %.2x but received %.2x\n", 0x00, next_byte);
		return;
	} else {
		printf("%.2x ", next_byte);
	}
	sendAck(pio);
	sendByte(pio, smWriteDat, R_PRESSED);	// fake right d-pad being pressed

	next_byte = pio_sm_get_blocking(pio, smReadCmd) >> 24;	// shift to get MSB
	if(next_byte != 0x00) {
		printf("\nWaiting for %.2x but received %.2x\n", 0x00, next_byte);
		return;
	} else {
		printf("%.2x ", next_byte);
	}
	sendAck(pio);
	sendByte(pio, smWriteDat, NO_PRESSED);	// other buttons are all inactive

	next_byte = pio_sm_get_blocking(pio, smReadCmd) >> 24;	// shift to get MSB
	if(next_byte != 0x00) {
		printf("\nWaiting for %.2x but received %.2x\n", 0x00, next_byte);
		return;
	} else {
		printf("%.2x ", next_byte);
	}
	// no ack and no send, transmission is over		
}

int main() {
	stdio_init_all();
	PIO pio = pio0;

	uint offsetReadCmd = pio_add_program(pio, &readCmd_program);
	uint offsetSendAck = pio_add_program(pio, &sendAck_program);
	uint offsetWriteDat = pio_add_program(pio, &writeDat_program);

	smReadCmd = pio_claim_unused_sm(pio, true);
	smSendAck = pio_claim_unused_sm(pio, true);
	smWriteDat = pio_claim_unused_sm(pio, true);

	sendAck_program_init(pio, smSendAck, offsetSendAck, PIN_ACK);
	writeDat_program_init(pio, smWriteDat, offsetWriteDat, PIN_DAT, PIN_SEL);
	readCmd_program_init(pio, smReadCmd, offsetReadCmd, PIN_CMD);

	/*
	selMonitor_program_init(pio, smSelMonitor, offsetSelMonitor, PIN_SEL);

	irq_set_exclusive_handler(PIO0_IRQ_0, pio0_irq0);
	pio_set_irq0_source_enabled(pio, pis_interrupt0, true);
	irq_set_enabled(PIO0_IRQ_0, true);
	*/

	/*
	uint32_t i = 0;
	while(true) {
		if(i % 1000000 == 0) {
			sendAck(pio);
		}
		i++;
	}
	*/

	//printf("\nSetup done, starting loop...\n");

	pio_sm_set_enabled(pio, smReadCmd, true);
	while(true) {
		uint32_t item = pio_sm_get_blocking(pio, smReadCmd);
		item >>= 24;	// shift to get MSB

		/* PRINT CMD RECEIVED */
		printf("\n%.2x ", item);

		if(item == 0x01)
			process_joy_req(pio);

	}
}