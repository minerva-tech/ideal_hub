/*
 * clk_idt5p49v6914a.c
 *
 *  Created on: Nov 8, 2016
 *      Author: root
 */
/*
	All "main" parts are same:
	PFD = 50M Hz
	VCO Freq. = 2500 MHz
	BW = 389.94 kHz


	Fx - desired frequency (what we need on the output)
	N = ( (VCO Freq) / 2 ) / Fx
	OD_intdiv (0x2D,0x2E) = INT(N)
	OD_offset (0x22,23,24,25) = 2^24 * FRAC(N)

	Example: 30MHz, SSCE=0 ( Bit 1 of the 0x25 is 0 )
	N = ( 2500 / 2 )  / 30 = 41.666666666
	OD_intdiv = 41 ( 0x29 ); 0x2D=90h, 0x2E=02h ;
		0x2E,0x2D = 0029h<<4 = 0x0290

	OD_offset = 2^24 * 0.66666666 = 11184810,666666666666666666666667; 11184811=0xAAAAAB;
	 	0x25,24,23,22 = 0x0xAAAAAB << 2 = 0x2AAAAAC

	0x20 = 00h 			- unused
	0x21 = 81h 			- output divider control
	0x22,23,24,25 = calculate	- output divider fraction [ OD_offset ]
	0x26,27,28 = 00h 00h 00h	- output divider step spread configuration register [ OD_step ]
	0x29 = 00h			- spread modulation rate [ OD_period ]
	0x2A = 04h                      - spread modulation rate
	0x2B = 00h                      - output skew integer part
	0x2C = 01h                      - output skew integer part
	0x2D,2E = calculate             - output divider integer part [ OD_intdiv ]
	0x2F = 00h                      - output skew fractional part
*/

#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <linux/i2c-dev.h>

#define IDT5P49V6914_I2C_ADDR	(0xD4>>1)

typedef unsigned char u8;

#define VCO_FREQ	(2500000000.0) /* Hz */

/*
static u8 idt_cfg_base[] = {
		0x61, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x01, 0xC0, 0x00, 0xB6, 0xB4, 0x92,
		0x88, 0x0C, 0x81, 0x80, 0x00, 0x03, 0x8C, 0x03, 0x20, 0x00, 0x00, 0x00, 0x9F, 0xFF, 0xF0, 0x80
};
static u8 idt_cfg_clk1[] = {
		0x00, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00
};
static u8 idt_cfg_clk2[] = {
		0x00, 0x0C, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x01, 0x00, 0xC0, 0x00
};
static u8 idt_cfg_clk3[] = {
		0x00, 0x0C, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xC0, 0x00
};
static u8 idt_cfg_clk4[] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00
};
static u8 idt_cfg_output[] = {
		0x13, 0x01, 0x13, 0x01, 0x13, 0x01, 0xB3, 0x00, 0xFE, 0x74
};
///
static u8 idt_cfg_clk1_100[] = {
		0x00, 0x81, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x01, 0x00, 0xC0, 0x00
};
static u8 idt_cfg_clk1_125[] = {
		0x00, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x01, 0x00, 0xA0, 0x00
};
static u8 idt_cfg_clk1_200[] = {
		0x00, 0x81, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x01, 0x00, 0x60, 0x00
};
*/

/*
static u8 cfg_lvpecl25v_100M[] = {
	0x61, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x01, 0xC0, 0x00, 0xB6, 0xB4, 0x92,
	0x88, 0x0C, 0x81, 0x80, 0x00, 0x03, 0x8C, 0x03, 0x20, 0x00, 0x00, 0x00, 0x9F, 0xFF, 0xF0, 0x80,
	0x00, 0x81, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x01, 0x00, 0xC0, 0x00,
	0x00, 0x0C, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x01, 0x00, 0xC0, 0x00,
	0x00, 0x0C, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xC0, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x13, 0x01, 0x13, 0x01, 0x13, 0x01, 0xB3, 0x00, 0xFE, 0x74
};

static u8 cfg_lvpecl25v_125M[] =
{
	0x61, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x01, 0xC0, 0x00, 0xB6, 0xB4, 0x92,
	0x88, 0x0C, 0x81, 0x80, 0x00, 0x03, 0x8C, 0x03, 0x20, 0x00, 0x00, 0x00, 0x9F, 0xFF, 0xF0, 0x80,
	0x00, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x01, 0x00, 0xA0, 0x00,
	0x00, 0x0C, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x01, 0x00, 0xC0, 0x00,
	0x00, 0x0C, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xC0, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x13, 0x01, 0x13, 0x01, 0x13, 0x01, 0xB3, 0x00, 0xFE, 0x74
};

static u8 cfg_lvpecl25v_200M[] =
{
	0x61, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x01, 0xC0, 0x00, 0xB6, 0xB4, 0x92,
	0x88, 0x0C, 0x81, 0x80, 0x00, 0x03, 0x8C, 0x03, 0x20, 0x00, 0x00, 0x00, 0x9F, 0xFF, 0xF0, 0x80,
	0x00, 0x81, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x01, 0x00, 0x60, 0x00,
	0x00, 0x0C, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x01, 0x00, 0xC0, 0x00,
	0x00, 0x0C, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xC0, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x13, 0x01, 0x13, 0x01, 0x13, 0x01, 0xB3, 0x00, 0xFE, 0x74
};
*/

static u8 cfg_lvpecl25v_250M[] =
{
	0x61, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x01, 0xC0, 0x00, 0xB6, 0xB4, 0x92,/* trim */
	0x88, 0x0C, 0x81, 0x80, 0x00, 0x03, 0x8C, 0x03, 0x20, 0x00, 0x00, 0x00, 0x9F, 0xFF, 0xF0, 0x80,/* main */
	0x00, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x01, 0x00, 0x50, 0x00,/* clk1 */
	0x00, 0x0C, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x01, 0x00, 0xC0, 0x00,/* clk2 */
	0x00, 0x0C, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xC0, 0x00,/* clk3 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,/* clk4 */
	0x13, 0x01, 0x13, 0x01, 0x13, 0x01, 0xB3, 0x00, 0xFE, 0x74									   /* output */
};

static u8 cfg_lvpecl25v_250M_shutdown[] =
{
	0x61, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x01, 0xC0, 0x00, 0xB6, 0xB4, 0x92,/* trim */
	0x83, 0x0C, 0x81, 0x80, 0x00, 0x03, 0x8C, 0x03, 0x20, 0x00, 0x00, 0x00, 0x9F, 0xFF, 0xF0, 0x80,/* main */
	0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x01, 0x00, 0x50, 0x00,/* clk1 */
	0x00, 0x80, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x01, 0x00, 0xC0, 0x00,/* clk2 */
	0x00, 0x80, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xC0, 0x00,/* clk3 */
	0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,/* clk4 */
	0x30, 0x01, 0x30, 0x01, 0x30, 0x01, 0x30, 0x00, 0x00, 0x74									   /* output */
};

static const int cfg_vco[] = { 2500, 2500, 2550, 2500, 2500, 2550, 2800, 2500, 2700, 2500, 2750, 2550, 2600, 2800, 2550, 2600, 2550, 2700, 2850, 2500, 2500, 2750, 2500, 2700, 2500 };


static int fdiic;
static u8 SendBuffer[128];

static void get_OD_intdiv_and_offset(int frq_hz, int *intdiv, int *offset)
{
	double N;
	int i,o,n;

	n = frq_hz/10000000 - 1;
	N = ((double)cfg_vco[n] * 1000000.0 * 0.5) / (double)frq_hz;
	i = (int)N;
	N = (N-(double)i)* 16777216.0;// stupid fucking complier. unable to do (double)(2<<24).
	o = (int)N;
	*intdiv = i;
	*offset = o;
}

int clk_open(void)
{
	int Status = 0;
	fdiic = open("/dev/i2c-0", O_RDWR);
	if (Status < 0)
	{
		printf("\n Unable to open I2C device\n");
		return -1;
	}
	return 0;
}
void clk_close()
{
	close(fdiic);
}
int clk_set_mhz(int MHz)
{
	int i,Status,transfer_size,BytesTransferred;
	u8* table = NULL;

	//100,125,200,250
	switch(MHz){
	case   0:  // Shutdown of IDT5P49V6914
		/* 
			To enter shutdown through i2c.
			Read IDT_VC5-Reg-Desc_MAU_2015123.pdf,  Page 16:
			0x30 to 0x60,0x62,0x64,0x66
			0x00 to 0x68
			0x83 to 0x10
			0x80 to 0x21,0x31,0x41,0x51	
		*/
		table = cfg_lvpecl25v_250M_shutdown;
		break;
	default:
		{
			int ODi, ODo, VCOi;
			table = cfg_lvpecl25v_250M;
			VCOi = cfg_vco[(MHz/10)-1] / 50;
			get_OD_intdiv_and_offset(MHz*1000000, &ODi, &ODo);

			table[0x25] = (u8)((ODo<< 2) & 0x000000FC) | (table[0x25] & 0x03);
			table[0x24] = (u8)((ODo>> 6) & 0x000000FF);
			table[0x23] = (u8)((ODo>>14) & 0x000000FF);
			table[0x22] = (u8)((ODo>>22) & 0x000000FF);

			table[0x2E] = (u8)((ODi<< 4) & 0x000000F0);
			table[0x2D] = (u8)((ODi>> 4) & 0x000000FF);

			table[0x17] = (u8)((VCOi>>4) & 0x000000FF);
			table[0x18] = (u8)((VCOi<<4) & 0x000000F0) | (table[0x18] & 0x0F);

			table[0x19] = 0;
			table[0x1A] = 0;
			table[0x1B] = 0;

			//printf("F=%d, VCOi=%d, ODi=%d ODo=%08xh[%7.5f]  \n",MHz, VCOi, ODi, ODo, (double)ODo/(double)(2<<24));
			//printf("17:%02x 18:%02x 22:%02x 23:%02x 24:%02x 25:%02x  2D:%02x 2E:%02x\n",table[0x17], table[0x18], table[0x22], table[0x23], table[0x24], table[0x25], table[0x2D], table[0x2E]);
		}
		break;
	}

	Status = ioctl(fdiic, I2C_SLAVE_FORCE, IDT5P49V6914_I2C_ADDR);
	if (Status < 0)
	{
		printf("\n Unable to set the IDT Clock address\n");
		return -1;
	}
	transfer_size = 91;
	SendBuffer[0] = 0x10;//program registers from 0x10 to 0x69. Set start address.
	for (i = 0; i<90; i++){
		SendBuffer[1+i] = table[0x10+i];
	}
	BytesTransferred = write(fdiic, SendBuffer, transfer_size);

	if(BytesTransferred==transfer_size)return 0;
	printf("IDT Clock ERROR: Bytes rwritten to IDT clock: %d (must be %d)\n",BytesTransferred,transfer_size);
	return -1;
}

int clk_shutdown()
{
	return clk_set_mhz(0);
}


