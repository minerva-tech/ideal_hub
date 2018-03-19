/*
 * pins.c
 *
 *  Created on: Nov 8, 2016
 *      Author: katas
 */
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <assert.h>

#define HEARTBIT_LED 		(40)	/* output */
#define MASTER_LED 			(54+9)	/* output */
#define RDY_OUT_GPIO		(54+1)	/* output */
#define MCLK_SEL_GPIO		(54+2)	/* output */

#define RDY_IN_GPIO			(54+0)	/* input */
#define SLOT_0_GPIO			(54+3)	/* input */
#define SLOT_1_GPIO			(54+4)	/* input */
#define SLOT_2_GPIO			(54+5)	/* input */
#define SLOT_3_GPIO			(54+6)	/* input */
#define MASTER_N_GPIO		(54+7)	/* input */
#define BACKPLANE_N_GPIO	(54+8)	/* input */

typedef struct{
	int index;// for iDeal_dbg fpga design first index is 906
	int direction;//0 -  input, 1 - output
	int value;
} gpio_t;


#ifdef IDEAL_HUB

static gpio_t pin[] = {
   /* index, direction, value */
	{ 906 + 40 + 0, 1, 1 }, //  0 HEARTBIT LED/LED1
	{ 906 + 54 + 9, 1, 0 }, //  1 MASTER LED/LED2
	{ 906 + 54 + 0, 0, 0 }, //  2 RDY_IN
	{ 906 + 54 + 1, 0, 0 }, //  3 RDY_OUT
	{ 906 + 54 + 2, 1, 1 }, //  4 MCLK_SEL
	{ 906 + 54 + 3, 0, 0 }, //  5 SLOT 0
	{ 906 + 54 + 4, 0, 0 }, //  6 SLOT 1
	{ 906 + 54 + 5, 0, 0 }, //  7 SLOT 2
	{ 906 + 54 + 6, 0, 0 }, //  8 SLOT 3
	{ 906 + 54 + 7, 0, 0 }, //  9 MASTER_N
	{ 906 + 54 + 8, 0, 0 }, // 10 BACKPLANE_N
};
#else

static gpio_t pin[] = {
   /* index, direction, value */
	{ 906 + 40 + 0, 1, 1 }, //  0 HEARTBIT LED/LED1
	{ 906 + 54 + 9, 1, 0 }, //  1 MASTER LED/LED2
	{ 906 + 54 + 0, 0, 0 }, //  2 RDY_IN
	{ 906 + 54 + 1, 1, 0 }, //  3 RDY_OUT
	{ 906 + 54 + 2, 1, 1 }, //  4 MCLK_SEL
	{ 906 + 54 + 3, 0, 0 }, //  5 SLOT 0
	{ 906 + 54 + 4, 0, 0 }, //  6 SLOT 1
	{ 906 + 54 + 5, 0, 0 }, //  7 SLOT 2
	{ 906 + 54 + 6, 0, 0 }, //  8 SLOT 3
	{ 906 + 54 + 7, 0, 0 }, //  9 MASTER_N
	{ 906 + 54 + 8, 0, 0 }, // 10 BACKPLANE_N
	{ 906 + 40 + 1, 1, 1 }, // 11 DBG-1
	{ 906 + 40 + 3, 1, 1 }, // 12 DBG-2
	{ 906 + 40 + 5, 1, 1 }, // 13 DBG-3
	{ 906 + 40 + 7, 1, 1 }, // 14 DBG-4
};

#endif

static int valuefd[sizeof(pin)/sizeof(gpio_t)];

/*
static struct{
	int backplane;
	int master;
	int slot;
	int ready_i;
	int ready_o;
	int mclk_sel;
} pins_state;
*/

static int get_pin(int index)
{
	char value;

	assert(index>-1&&index<(sizeof(pin)/sizeof(gpio_t)));

	read(valuefd[index], &value, 1);
	if ('0' == value)
		pin[index].value = 0;
	else
		pin[index].value = 1;
	return pin[index].value;
}
static void set_pin(int index, int value)
{
	pin[index].value = value ? 1 : 0;
	write(valuefd[index], pin[index].value ? "1" : "0", 2);
}
int pins_is_backplane(void)
{
	return get_pin(10)?0:1;
}
int pins_is_master(void)
{
	return get_pin(9)?0:1;
}
int pins_slot_id(void)
{
	int i,slot;
	for(i=0,slot=0;i<4;i++){
		slot += get_pin(5+i)<<i;
	}
	return slot;
}

int pin_rdy_i_get(void)
{
	/*
	char value;
	read(valuefd[2], &value, 1);
	if ('0' == value)
		pin[2].value = 0;
	else
		pin[2].value = 1;
	return pin[2].value;
	*/
	return get_pin(2);
}
void pin_rdy_o_set(int value)
{
	//pin[3].value = value ? 1 : 0;
	//write(valuefd[3], pin[3].value ? "1" : "0", 2);
	set_pin(3, value);
}
void pin_mclk_set(int value)
{
	//pin[4].value = value ? 1 : 0;
	//write(valuefd[4], pin[4].value ? "1" : "0", 2);
	set_pin(4, value);
}
void pin_led_set(int led, int value)
{
	assert(led>0&&led<3);
	//pin[led-1].value = value ? 1 : 0;
	//write(valuefd[led-1], pin[led-1].value ? "1" : "0", 2);
	set_pin(led-1, value);
}

//led: 1,2,3,4
void pin_led_dbg_set(int led, int value)
{
	set_pin(11+led-1, value);
}

int pins_open(void)
{
	int fd;
	int i;
	char text[100], value;
	////
	printf("GPIO test running...\n");
	fd = open("/sys/class/gpio/export", O_WRONLY);
	if (fd < 0)
	{
		printf("Cannot open GPIO to export it\n");
		return -1;
	}
	for (i = 0; i < sizeof(pin)/sizeof(gpio_t); i++){
		sprintf(text,"%d",pin[i].index);
		write(fd,text,sizeof(text)+1);
	}
	close(fd);
	printf("GPIO exported successfully\n");

	for (i = 0; i < sizeof(pin)/sizeof(gpio_t); i++){
		sprintf(text, "/sys/class/gpio/gpio%d/direction", pin[i].index);
		fd = open(text, O_RDWR);
		if (fd < 0)
		{
			printf("Cannot open GPIO direction it\n");
			return -1;
		}
		write(fd, pin[i].direction ? "out" : "in", pin[i].direction ? 4 : 3);
		close(fd);
	}
	printf("GPIOs directions set as output successfully\n");

	for (i = 0; i < sizeof(pin)/sizeof(gpio_t); i++){
		sprintf(text, "/sys/class/gpio/gpio%d/value", pin[i].index);
		valuefd[i] = open(text, O_RDWR);
		if (valuefd[i] < 0)
		{
			printf("Cannot open GPIO value\n");
			return -1;
		}
		if (pin[i].direction == 1){
			write(valuefd[i], pin[i].value?"1":"0", 2);
		}
		else{
			read(valuefd[i], &value, 1);
			if ('0' == value)
				pin[i].value = 0;
			else
				pin[i].value = 1;
		}
	}
	return 0;
}
int pins_close()
{
	int i;
	for(i = 0; i < sizeof(pin)/sizeof(gpio_t); i++){
		close(valuefd[i]);
		valuefd[i] = NULL;
	}
	return 0;
}


