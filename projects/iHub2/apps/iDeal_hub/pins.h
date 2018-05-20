/*
 * pins.h
 *
 *  Created on: Nov 8, 2016
 *      Author: katas
 */

#ifndef PINS_H_
#define PINS_H_

#define HEARTBIT 1
#define MASTER 2

#define ON 1
#define OFF 0

int pins_open(void);
int pins_close(void);
int pins_is_backplane(void);
int pins_is_master(void);
int pins_slot_id(void);

void pin_led_set(int led, int value);
void pin_mclk_set(int value);
void pin_rdy_o_set(int value);
int pin_rdy_i_get(void);

void pin_led_dbg_set(int led, int value);

#endif /* PINS_H_ */
