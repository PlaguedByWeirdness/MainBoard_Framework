#include "actuator.h"
#include "can.h"
#include "gpio.h"
#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>
#include <stdio.h>


uint8_t buffer[8];
uint8_t return_buffer[8];

uint8_t module_mosfet_status[3][3] = {
		{1, false, MOSFET_1_PIN},
		{2, false, MOSFET_2_PIN},
		{3, false, MOSFET_3_PIN}
};
uint8_t module_relay_status = 0;

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

/*
 * WARNING:
 * the can.c arent the same at actuator and here
 */
static void _actuator(uint8_t *buffer, uint8_t *return_buffer) {

	while(CAN_Write(buffer, DRIVER_LIFT_TX_IDENTIFICATOR))
		_delay_ms(10);

	CAN_Read(return_buffer, DRIVER_LIFT_RX_IDENTIFICATOR);

}

/*
 * 	Function:    static void buffer_load(uint8_t tool, uint8_t tool_id, uint8_t tool_function, int16_t value)
 * 	Description: it loads the buffer with the parameters
 */
static void buffer_load(uint8_t tool,
						uint8_t tool_id,
						uint8_t tool_function,
						int16_t value) {

	buffer[0] = tool;				// the tool (AX, MOSFET, RELAY)
	buffer[1] = tool_id;			// the id of the tool (ID of an AX, MOSFET, RELAY)
	buffer[2] = tool_function;		// what function (move to position, turn on off)
	buffer[3] = value >> 8;			// 16bit value, good for percision with servos
	buffer[4] = value & 0xFF;

}

/*
 * 	Function: 	 _actuator_ping()
 * 	Description: it pings and checks if the communication with the actuator board is present
 */
uint8_t _actuator_ping() {

	buffer_load('P', 0, 0, 0);			// TOOL: Ping

	_actuator(buffer, return_buffer);	// Sending the buffer to the actuator and receiving the return one

	// if it successful it will return 'P'
	if(return_buffer[0] == 'P') {
		return 1;
	} else {
		return 0;
	}

}

/*
 * 	Function:    uint8_t ax_set_angle(uint8_t id, uint16_t angle)
 * 	Description: It send the goal angle for the ax
 * 	Paramteres:  uint8_t id -> the id of the servo, uint16_t angle -> the angle it will go to
 */
uint8_t ax_set_angle(uint8_t id, uint16_t angle) {

	uint8_t status;						// return status

	buffer_load('A', id, 'A', angle);	// TOOL: 'AX', ID_TOOL: id, FUNC_TOOL: Angle, VALUE: angle
	_actuator(buffer, return_buffer);	// Sending the buffer to the actuator and receiving the return one
	status = return_buffer[0];			// Do something with the buffer

	// we are returning the first return_buffer index because it will say if it was successful or not
	return status;
}

/*
 * 	Function:    uint8_t ax_set_speed(uint8_t id, uint16_t speed)
 * 	Description: It sends the goal speed for the ax
 * 	Parameters:  uint8_t id -> the id of the servo, uint16_t speed -> the goal speed
 */
uint8_t ax_set_speed(uint8_t id, uint16_t speed) {

	uint8_t status;						// return status

	buffer_load('A', id, 'S', speed);	// TOOL: 'AX', ID_TOOL: id, FUNC_TOOL: Speed, VALUE: speed
	_actuator(buffer, return_buffer);	// Sending the buffer to the actuator and receiving the return one
	status = return_buffer[0];			// Do something with the buffer

	// we are returning the first return_buffer index because it will say if it was successful or not
	return status;
}

/*
 * 	Function:    uint8_t ax_get_status(uint8_t id, uint8_t return_option)
 * 	Description: It gets the status of a servo
 * 	Parameters:  uint8_t id -> the id of the ax, uint8_t return_option -> the type of status it will return
 */
uint8_t ax_get_status(uint8_t id, uint8_t return_option) {

	/*
	 * 	return_option:
	 * 	0 -> temperature
	 * 	1 -> voltage
	 * 	2 -> current position
	 * 	3 -> moving?
	 */

	buffer_load('A',id, 'S', 0);			// TOOL: 'AX', ID_TOOL: id, FUNC_TOOL: Status, VALUE: 0
	_actuator(buffer, return_buffer);		// Sending the buffer to the actuator and receiving the return one

	return return_buffer[return_option];	// return a buffer index dependent on the return_option value

}

/*
 * 	Function: 	 uint8_t mosfet_set(uint8_t id, uint16_t status)
 * 	Description: ON/OFF for the mosfet on the actuator board
 * 	Paramters:   uint8_t id -> the id of the mosfet, uint16_t status -> ON/OFF
 */
uint8_t mosfet_set(uint8_t id, uint16_t status) {

	buffer_load('M', id, 'C', status);		// TOOL: Mosfet, ID_TOOL: id, FUNC_TOOL: Condition, VALUE: status
	_actuator(buffer, return_buffer);		// Sending the buffer to the actuator and receiving the return one

	status = return_buffer[0];				// Get the status/return of the actuator board

	return status;

}

/*
 * 	Function: 	 uint8_t mosfet_status(uint8_t id)
 * 	Description: Returns the status of the id mosfet
 * 	Paramters:   uint8_t id -> the id of the mosfet
 */
uint8_t mosfet_status(uint8_t id) {

	buffer_load('M',id, 'S', 0);			// TOOL: Mosfet, ID_TOOL: id, FUNC_TOOL: Status, VALUE: 0
	_actuator(buffer, return_buffer);		// Sending the buffer to the actuator and receiving the return one

	return return_buffer[0];				// return the status of it

}

/*
 * 	Function: 	 uint8_t relay_set(uint8_t id, uint16_t status)
 * 	Description: ON/OFF for the relay on the actuator board
 * 	Parameters:  uint8_t id -> the id of the relay, uint16_t status -> ON/OFF
 */
uint8_t relay_set(uint8_t id, uint16_t status) {

	buffer_load('R', id, 'C', status);		// TOOL: Relay, ID_TOOL: id, FUNC_TOOL: Condition, VALUE: status
	_actuator(buffer, return_buffer);		// Sending the buffer to the actuator and receiving the return one

	status = return_buffer[0];				// Get the status/return of the actuator board

	return status;
}

/*
 * 	Function: 	 uint8_t relay_status(uint8_t id)
 * 	Description: Returns the status of the id relay
 * 	Parameters:  uint8_t id -> the id of the relay
 */
uint8_t relay_status(uint8_t id) {

	buffer_load('R',id, 'S', 0);			// TOOL: Relay, ID_TOOL: id, FUNC_TOOL: Status, VALUE: 0
	_actuator(buffer, return_buffer);		// Sending the buffer to the actuator and receiving the return one

	return return_buffer[0];				// return the status of it
}

/*
 * 	Function: 	 void module_init(unsigned char version)
 * 	Description: Init the EP_Module
 * 	Parameters:  unsigned char version -> the version we want to use
 */
void module_init(unsigned char version) {

	/* https://github.com/Elektropioneer/EP_Module */

	/*
	 * 	Version:
	 * 	0 -> mosfet module
	 * 	1 -> relay module
	 */

	if(version) {
		// relay module
		gpio_register_pin(RELAY_PIN, GPIO_DIRECTION_OUTPUT, false);		// PG0

	} else {
		// mosfet module
		gpio_register_pin(48, GPIO_DIRECTION_OUTPUT, false);			// PG0
		gpio_register_pin(49, GPIO_DIRECTION_OUTPUT, false);			// PG1
		gpio_register_pin(50, GPIO_DIRECTION_OUTPUT, false);			// PG2

	}
}

/*
 * 	Function:    void module_set_relay_status(bool status)
 * 	Description: sets the relay to the parameter status and updates the "database"
 * 	Parameters:  bool status -> the status we want to set it to
 */
void module_set_relay_status(bool status) {

	gpio_write_pin(RELAY_PIN, status);
	module_relay_status = status;

}

/*
 * 	Function:    bool module_read_relay_status()
 * 	Description: returns the status of the relay
 */
bool module_read_relay_status() { return module_relay_status; }

/*
 * 	Function:    void module_set_mosfet_status(uint8_t id, bool status)
 * 	Description: sets the mosfet by the id to the parameter status and updates the "database"
 * 	Parameters:  uint8_t id -> the id of the mosfet we want to updatebool status -> the status we want to set it to
 */
void module_set_mosfet_status(uint8_t id, bool status) {

	gpio_write_pin(module_mosfet_status[id-1][2], status);
	module_mosfet_status[id-1][1] = status;

}

/*
 * 	Function:    bool module_read_mosfet_status(uint8_t id)
 * 	Description: return the status of the id of the mosfet
 */
bool module_read_mosfet_status(uint8_t id) { return module_mosfet_status[id-1][1]; }




