/*
 * clk_idt5p49v6914a.h
 *
 *  Created on: Nov 8, 2016
 *      Author: root
 */

#ifndef CLK_IDT5P49V6914A_H_
#define CLK_IDT5P49V6914A_H_

int clk_open(void);
int clk_close(void);
int clk_set_mhz(int MHz);
int clk_shutdown();

#endif /* CLK_IDT5P49V6914A_H_ */
