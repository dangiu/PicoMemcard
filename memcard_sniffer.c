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

#define BUFF_LEN 4096

PIO pio = pio0;

uint smSelMonitor;
uint smCmdReader;
uint smDatReader;

uint offsetSelMonitor;
uint offsetCmdReader;
uint offsetDatReader;

uint32_t buffIndex = 0;
uint8_t cmdBuffer[BUFF_LEN];
uint8_t datBuffer[BUFF_LEN];

bool restartProto = false;

/**
 * @brief Interrupt handler called when SEL goes high
 * Resets cmdReader and datWriter state machines
 */
void pio0_irq0() {
	pio_set_sm_mask_enabled(pio, 1 << smCmdReader | 1 << smDatReader, false);
	pio_restart_sm_mask(pio, 1 << smCmdReader | 1 << smDatReader);
	pio_sm_exec(pio, smCmdReader, pio_encode_jmp(offsetCmdReader));	// restart smCmdReader PC
	pio_sm_exec(pio, smDatReader, pio_encode_jmp(offsetDatReader));	// restart smDatReader PC

	pio_interrupt_clear(pio0, 0);
	pio_enable_sm_mask_in_sync(pio, 1 << smCmdReader | 1 << smDatReader);
	restartProto = true;
}

void suspend_proto() {
	pio_set_sm_mask_enabled(pio, 1 << smCmdReader | 1 << smDatReader, false);
	pio_restart_sm_mask(pio, 1 << smCmdReader | 1 << smDatReader);
}

void restart_proto() {
	pio_sm_clear_fifos(pio, smCmdReader);
	pio_sm_clear_fifos(pio, smDatReader);
	pio_sm_exec(pio, smCmdReader, pio_encode_jmp(offsetCmdReader));	// restart smCmdReader PC
	pio_sm_exec(pio, smDatReader, pio_encode_jmp(offsetDatReader));	// restart smDatReader PC
	pio_enable_sm_mask_in_sync(pio, 1 << smCmdReader | 1 << smDatReader);
}

int main() {
	stdio_init_all();

	printf("\n\nBeginning Execution...\n");

	/* Setup PIO interrupts */
	irq_set_exclusive_handler(PIO0_IRQ_0, pio0_irq0);
	irq_set_enabled(PIO0_IRQ_0, true);

	offsetSelMonitor = pio_add_program(pio, &sel_monitor_program);
	offsetCmdReader = pio_add_program(pio, &cmd_reader_program);
	offsetDatReader = pio_add_program(pio, &dat_reader_program);

	smSelMonitor = pio_claim_unused_sm(pio, true);
	smCmdReader = pio_claim_unused_sm(pio, true);
	smDatReader = pio_claim_unused_sm(pio, true);

	dat_reader_program_init(pio, smDatReader, offsetDatReader, PIN_DAT);
	cmd_reader_program_init(pio, smCmdReader, offsetCmdReader, PIN_CMD);
	sel_monitor_program_init(pio, smSelMonitor, offsetSelMonitor, PIN_SEL);


	/* Enable all SM simultaneously */
	uint32_t smMask = (1 << smSelMonitor) | (1 << smCmdReader) | (1 << smDatReader);
	pio_enable_sm_mask_in_sync(pio, smMask);

	int index = 0;

	while(true) {
		cmdBuffer[index] = read_cmd_byte_blocking(pio, smCmdReader);
		datBuffer[index] = read_cmd_byte_blocking(pio, smDatReader);
		index++;
		if(index >= BUFF_LEN) {
			break;
		}
	}

	for(int i = 0; i < BUFF_LEN; i++) {
		printf("\t%.2x\t%.2x\n", cmdBuffer[i], datBuffer[i]);
	}
}