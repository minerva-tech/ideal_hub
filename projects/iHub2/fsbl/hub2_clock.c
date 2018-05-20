/*
*/
#include "xiicps.h"
#include "xil_printf.h"
#include "assert.h"
//#include "hub2_commander.h"

#define IIC_SLAVE_ADDR		(0xD4>>1)
#define IIC_SCLK_RATE		100000
#define TEST_BUFFER_SIZE	132
#define HUB2_COMMANDER_BASE_ADDR (0x43C40000)

static u8 SendBuffer[TEST_BUFFER_SIZE];    /**< Buffer for Transmitting Data */
static u8 RecvBuffer[TEST_BUFFER_SIZE];    /**< Buffer for Receiving Data */

static u8 clk_cfg[] = {
		0x61, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x01, 0xC0, 0x00, 0xB6, 0xB4, 0x92,
		0x88, 0x0C, 0x81, 0x80, 0x00, 0x03, 0x8C, 0x03, 0x20, 0x00, 0x00, 0x00, 0x9F, 0xFF, 0xF0, 0x80,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x50, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x50, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x13, 0x00, 0x13, 0x00, 0x13, 0x01, 0xB3, 0x00, 0x9E, 0x14
};


void clock_config(void)
{
		XIicPs_Config *Config;
		XIicPs Iic;
		int transfer_size, i;
		int Status, reg;

		//  Set MCLK_SEL to 1: board internal clock source select
		//reg = HUB2_COMMANDER_mReadReg(HUB2_COMMANDER_BASE_ADDR, 0);// reg = *(volatile u32 *)HUB2_COMMANDER_BASE_ADDR;
		//reg = reg | (1<<6);
		//HUB2_COMMANDER_mWriteReg(HUB2_COMMANDER_BASE_ADDR, 0, reg);

		Config = XIicPs_LookupConfig(XPAR_XIICPS_0_DEVICE_ID);
		assert(Config != NULL);

		Status = XIicPs_CfgInitialize(&Iic, Config, Config->BaseAddress);
		assert(Status == XST_SUCCESS);

		Status = XIicPs_SelfTest(&Iic);
		assert(Status == XST_SUCCESS);

		XIicPs_SetSClk(&Iic, IIC_SCLK_RATE);

		for (i = 0; i < TEST_BUFFER_SIZE; i++) {
			SendBuffer[i] = (i % TEST_BUFFER_SIZE);
			RecvBuffer[i] = 0;
		}

		// read
		transfer_size = 1;
		SendBuffer[0] = 0x00;//program registers from 0x10 to 0x69. Set start address.
		Status = XIicPs_MasterSendPolled(&Iic, SendBuffer,transfer_size, IIC_SLAVE_ADDR);
		assert(Status == XST_SUCCESS);

		while (XIicPs_BusIsBusy(&Iic)){};


		transfer_size = 0x69-0x00+0x01;
		Status = XIicPs_MasterRecvPolled(&Iic, RecvBuffer,transfer_size, IIC_SLAVE_ADDR);
		assert(Status == XST_SUCCESS);

		for(i = 0; i < transfer_size; i ++) {
			 xil_printf("0x%02x ", 0x10+i);
		}
		xil_printf("\n");
		for(i = 0; i < transfer_size; i ++) {
			 xil_printf("0x%02x ", RecvBuffer[i]);
		}

		xil_printf("\n\nProgram\n\n");
		// program
		transfer_size = 91;
		SendBuffer[0] = 0x10;//program registers from 0x10 to 0x69. Set start address.
		for(i=0;i<90;i++){
			SendBuffer[1+i] = clk_cfg[0x10+i];
			//
		}
		Status = XIicPs_MasterSendPolled(&Iic, SendBuffer,transfer_size, IIC_SLAVE_ADDR);
		assert(Status == XST_SUCCESS);

		while (XIicPs_BusIsBusy(&Iic)){};

		// verify
		transfer_size = 1;
		SendBuffer[0] = 0x00;//program registers from 0x10 to 0x69. Set start address.
		Status = XIicPs_MasterSendPolled(&Iic, SendBuffer,transfer_size, IIC_SLAVE_ADDR);
		assert(Status == XST_SUCCESS);

		transfer_size = 0x69-0x00+0x01;
		Status = XIicPs_MasterRecvPolled(&Iic, RecvBuffer,transfer_size, IIC_SLAVE_ADDR);
		assert(Status == XST_SUCCESS);
		for(i = 0; i < transfer_size; i ++) {
			 xil_printf("0x%02x ", 0x10+i);
		}
		xil_printf("\n");
		for(i = 0; i < transfer_size; i ++) {
			 xil_printf("0x%02x ", RecvBuffer[i]);
		}
		xil_printf("\n\n");
}
