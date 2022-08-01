#ifndef __PAD_H__
#define __PAD_H__

/* PSX Digital Controller bit position - 0 when pressed, 1 when released */
#define SELECT_POS		0
#define L3_POS			1	// analog mode only
#define R3_POS			2	// analog mode only
#define START_POS		3
#define UP_POS			4
#define RIGHT_POS		5
#define DOWN_POS		6
#define LEFT_POS		7
#define L2_POS			8
#define R2_POS			9
#define L1_POS			10
#define R1_POS			11
#define TRIANGLE_POS	12
#define CIRCLE_POS		13
#define X_POS			14
#define SQUARE_POS		15

#define SELECT		(uint16_t) ~(1 << SELECT_POS)
#define L3			(uint16_t) ~(1 << L3_POS)
#define R3			(uint16_t) ~(1 << R3_POS)
#define START		(uint16_t) ~(1 << START_POS)
#define UP			(uint16_t) ~(1 << UP_POS)
#define RIGHT		(uint16_t) ~(1 << RIGHT_POS)
#define DOWN		(uint16_t) ~(1 << DOWN_POS)
#define LEFT		(uint16_t) ~(1 << LEFT_POS)
#define L2			(uint16_t) ~(1 << L2_POS)
#define R2			(uint16_t) ~(1 << R2_POS)
#define L1			(uint16_t) ~(1 << L1_POS)
#define R1			(uint16_t) ~(1 << R1_POS)
#define TRIANGLE	(uint16_t) ~(1 << TRIANGLE_POS)
#define CIRCLE		(uint16_t) ~(1 << CIRCLE_POS)
#define X			(uint16_t) ~(1 << X_POS)
#define SQUARE		(uint16_t) ~(1 << SQUARE_POS)

#endif