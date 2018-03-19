/*
 * eeprom.c
 *
 *  Created on: Nov 8, 2016
 *      Author: Katas
 *
 *      EETPOM AT24C01 (1kbit, 128 Bytes, 128x8, 4-byte page write mode)
 *
 *      IPv4 ADDRESS  0.. 3 	(4)
 *      MAC ADDRESS   4..11		(8)
 *      IPv6 ADDRESS 12..27		(16)
 */
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <assert.h>
#include <linux/i2c-dev.h>

#define EEPROM_ADDR			(0xAE>>1)
#define PAGESIZE			(4)
#define EEPROM_SIZE			(128)

typedef unsigned char u8;
typedef unsigned char AddressType;

static int fdiic;

static u8 iic_write(u8 WriteBuffer[], u8 NoOfBytes)
{
	u8 BytesWritten;
	BytesWritten = write(fdiic, WriteBuffer, NoOfBytes);
	/*
	 * Wait till the EEPROM internally completes the write cycle.
	 */
	sleep(1);
	return BytesWritten;
}

static u8 iic_read(u8 ReadBuffer[], u8 NoOfBytes)
{
	u8 BytesRead;
	BytesRead = read(fdiic, ReadBuffer, NoOfBytes);
	return BytesRead;
}

int eeptom_open(void)
{
	int Status = 0;
	fdiic = open("/dev/i2c-0", O_RDWR);
	if (Status < 0)
	{
		printf("\n Unable to open I2C device\n");
		return -1;
	}

	Status = ioctl(fdiic, I2C_SLAVE_FORCE, EEPROM_ADDR);
	if(Status < 0)
	{
		printf("\n Unable to set the EEPROM address\n");
		return -1;
	}
	return 0;
}
void eeprom_close(void)
{
	close(fdiic);
}

