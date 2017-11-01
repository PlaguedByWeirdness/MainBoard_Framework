#include "actuator.h"
#include "can.h"
#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>
#include <stdio.h>


uint8_t buffer[8];
uint8_t return_buffer[8];

/*
 *	Function: 		void servo_init(unsigned int f_pwm)
 *	Parameters: 	unsigned int f_pwm
 *	Description: 	servo init
 */
void servo_init(unsigned int f_pwm) {
	DDRE |= ((1 << PINE3) | (1 << PINE4) | (1 << PINE5));
	DDRB |= (1 << PINB7);										//OCR0A 8bit

	TCNT3 = 0;
	TCNT0 = 0;

	OCR3A = 0;
	OCR3B = 0;
	OCR3C = 0;

	TCCR3A = (1 << COM3A1)  | (1 << COM3B1) | (1 << COM3B0) | (1 << COM3C1) | (1 << COM3C0) | (1 << WGM31);
	TCCR3B = (1<< CS31) | (1 << WGM32) | (1 << WGM33) ; 		// PRESKALER = 8

	//8bit timer
	TCCR0A = (1 << WGM01) | (1 << WGM00) | (1 << COM0A1) | (1 << CS01) | (1 << CS00);

	ICR3   = ((double)F_CPU) / (8.0 * f_pwm) + 0.5;
}

/*
 *	Function: 		static void servo_set_duty_cycle_one-two-three(int16_t value)
 *	Parameters: 	int16_t value - the pwm value
 *	Description: 	duty cycle for servos
 *	Pin:			PE3, PE4, PE5
 */
static void servo_set_duty_cycle_one(int16_t value) {
	uint16_t temp = ((double)ICR3 / 255.0) * value + 0.5;
	OCR3AH = temp >> 8;
	OCR3AL = temp & 0xFF;
}
static void servo_set_duty_cycle_two(int16_t value) {
	uint16_t temp = ((double)ICR3 / 255.0) * value + 0.5;
	OCR3BH = temp >> 8;
	OCR3BL = temp & 0xFF;
}
static void servo_set_duty_cycle_three(int16_t value) {
	uint16_t temp = ((double)ICR3 / 255.0) * value + 0.5;
	OCR3CH = temp >> 8;
	OCR3CL = temp & 0xFF;
}

/*
 * 	Function: 	 static int16_t range_conv(float angle)
 * 	Descritpion: this function converts 0-180 to int16_t value
 */
static int16_t range_conv(float angle) {
	return ((65535 * angle) / 180) - 32768;
}

/*
 * 	Function: 	 static char check_servo_range(float angle)
 * 	Description: returns a 1 if the range is good and a 0 if not
 */
static char check_servo_range(float angle) {
	if(angle >= 0.0 && angle <= 180.0)
		return 1;
	else
		return 0;
}

/*
 * 	Function: 	void servo_set_angle_one-two-three(uint8_t angle)
 * 	Descrition: the angle goes from 0.0 - 180.0
 */
void servo_set_angle_one(float angle) {
	if(check_servo_range(angle))
		servo_set_duty_cycle_one(range_conv(angle));
}

void servo_set_angle_two(float angle) {
	if(check_servo_range(angle))
		servo_set_duty_cycle_two(range_conv(angle));
}

void servo_set_angle_three(float angle) {
	if(check_servo_range(angle))
		servo_set_duty_cycle_three(range_conv(angle));
}

// this is a 8bit register (pwm)
void servo_set_angle_four(float angle) {
	if(check_servo_range(angle))
		OCR0A = (int)((255 * angle) / 180);
}

/*
 *  Test functions for actuator board communication
 */

/*
 * 		Protocol for actuator
 * 		buffer[0] - ID OF TOOL (ax, relay, mosfet)
 * 		buffer[1] - THE EXACT ID OF THE TOOL
 * 		buffer[2] - THE FUNCTION TO DO (position, move, turn on off)
 * 		buffer[3] - value >> 8
 * 		buffer[4] - value & 0xFF
 *
 */

static void _actuator(uint8_t *buffer, uint8_t *return_buffer) {

	while(CAN_Write(buffer, DRIVER_LIFT_TX_IDENTIFICATOR))
		_delay_ms(10);

	CAN_Read(return_buffer, DRIVER_LIFT_RX_IDENTIFICATOR);

}

static void buffer_load(uint8_t tool,
						uint8_t tool_id,
						uint8_t tool_function,
						int16_t value) {

	buffer[0] = tool;			// calling AX
	buffer[1] = tool_id;				// id
	buffer[2] = tool_function;			// set angle
	buffer[3] = value >> 8;
	buffer[4] = value & 0xFF;

}

uint8_t _actuator_ping() {

	buffer_load('P', 0, 0, 0);

	_actuator(buffer, return_buffer);

	if(return_buffer[0] == 'P') {
		return 1;
	} else {
		return 0;
	}

}

uint8_t ax_set_angle(uint8_t id, uint16_t angle) {

	uint8_t status;

	buffer_load('A', id, 'A', angle);

	_actuator(buffer, return_buffer);

	// do stuff with return buffer
	status = return_buffer[0];

	return status;

}

uint8_t ax_get_status(uint8_t id, uint8_t return_option) {

	/*
	 * 	return_option:
	 * 	0->temperature
	 * 	1->voltage
	 * 	2->current position
	 * 	3->moving?
	 */

	buffer_load('A',id, 'S', 0);

	_actuator(buffer, return_buffer);

	return return_buffer[return_option];

}



