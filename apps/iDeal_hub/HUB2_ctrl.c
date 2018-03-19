//
//	SET ADC
//	1. RX_DELAY	reg0(27 downto 0)
//	2. SAMPLES      reg1(17 downto 0)
//	3. CTRL         reg2
//	4. STATUS       reg3
//
//	SET TGC

//	1. CTRL		reg4(1..0) 0-parameters ready, 1-host do
//	5. ACTIVE 	reg5(0) 0-if set then do pulse
//	2. MODES        reg5(5..4)
//		reg5(4) direct access to tgc interface (via reg14)
//		reg5(5) ???
//	3. TX_DELAY	reg6(31 downto 0)
//	4. RATIO        reg7(7 downto 0)
//	6. SIZE(8..0)	reg8(8 downto 0)

//	7. TGC_0X0	reg9(15..0)
//	8. TGC_0X1	reg10(15..0)
//	9. TGC_0X2	reg11(15..0)
//	10. TGC_0X3	reg12(15..0)
//	11. TGC_0X4	reg13(15..0)
//
//	12. TGC REGISTER ADD(16+3..16+0) DATA(15..0) reg14
//
//
//	GET
//	
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/mman.h>
#include "HUB2_ctrl.h"

#define ADC_RX_DELAY_OFFSET 	(0)
#define ADC_SAMPLES_OFFSET	(1)

#define MAP_SIZE 	4096UL
#define MAP_MASK 	(MAP_SIZE - 1)

#define TGC_CTRL_CH0	0x43C50000
#define TGC_CTRL_CH1	0x43C70000
static int ctrl_fd[2] = {-1,-1};
static void* ctrl_mapped_base[2] = {NULL,NULL};
static void* ctrl_base[2] = {NULL,NULL};

#define TGC_TABLE_CH0	0x7AA00000
#define TGC_TABLE_CH1	0x7AA10000
static int table_fd[2] = {-1,-1};
static void* table_mapped_base[2] = {NULL,NULL};
static void* table_base[2] = {NULL,NULL};

int tgc_ctrl_open(int cn)
{
	unsigned int hw_base = cn==0 ? TGC_CTRL_CH0 : TGC_CTRL_CH1;
	unsigned int v;
 
	ctrl_fd[cn] = open("/dev/mem", O_RDWR | O_SYNC);
	if( !(ctrl_fd[cn] > -1) ){
		printf("Can't open dev-mem.\n");
		return -1;
	}

	ctrl_mapped_base[cn] = mmap( 
		0, 
		MAP_SIZE, 
		PROT_READ | PROT_WRITE, 
		MAP_SHARED, ctrl_fd[cn], 
		(off_t)(hw_base & ~MAP_MASK) 
	);

	if (ctrl_mapped_base[cn] == (void *) -1) {
		printf("Can't map the memory to user space.\n");
		return -1;
	}

	ctrl_base[cn] = ctrl_mapped_base[cn] + (hw_base & MAP_MASK);

	v = *((volatile unsigned int *) (ctrl_base[cn] + (16*4)));
	printf("Channel %d TGC controller IP Core version %08x\n", cn, v);
	return (int)ctrl_base[cn];
}

int tgc_ctrl_close(int cn)
{
	if (munmap(ctrl_mapped_base[cn], MAP_SIZE) == -1) {
		printf("Can't unmap memory from user space.\n");
		return -1;
	}else{
		close(ctrl_fd[cn]);
		return 0;
	}
}

inline void tgc_run(int cn)
{
	*((volatile unsigned int*) (ctrl_base[cn] + (4*4))) = 1;
}

inline void tgc_sw_do(int cn)
{
	unsigned int r;
	r = *((volatile unsigned int *) (ctrl_base[cn] + (4*4)));

	*((volatile unsigned int*) (ctrl_base[cn] + (4*4))) = r | 2;
}

inline void tgc_set_active(int cn, int active)
{
	unsigned int r;
	r = *((volatile unsigned int *) (ctrl_base[cn] + (5*4)));
	r = r & (~0x00000001);
	r = r | (active?0x00000001:0);
	*((volatile unsigned int*) (ctrl_base[cn] + (5*4))) = r;
}

inline void adc_set_delay(int cn, int rx_delay)
{
	*((volatile unsigned int*) (ctrl_base[cn] + (ADC_RX_DELAY_OFFSET*4))) = rx_delay;	
}
inline void adc_set_length(int cn, int samples_number)
{
	*((volatile unsigned int*) (ctrl_base[cn] + (ADC_SAMPLES_OFFSET*4))) = samples_number;	
}

inline void tgc_set_delay(int cn, int tx_delay)
{
        *((volatile unsigned int*) (ctrl_base[cn] + (6*4))) = tx_delay;
}
inline void tgc_set_length(int cn, int tgc_table_length)
{
	//assert(tgc_table_length<512);//TODO: extend table length
	assert(tgc_table_length<257);
	*((volatile unsigned int*) (ctrl_base[cn] + (8*4))) = tgc_table_length;
} 
inline void tgc_set_ratio(int cn, int ratio)// adc ticks per one tgc tick. must be even. It is constantly 10 by specification.
{
	assert(ratio<257);
	assert(0==(ratio&1));
	*((volatile unsigned int*) (ctrl_base[cn] + (7*4))) = ratio;
}
inline void tgc_set_reg(int cn, short addr, short value)// using reg9..reg13
{	
	*((volatile unsigned int*) (ctrl_base[cn] + ((9+addr)*4))) = value;
}

int tgc_table_open(int cn)
{
	unsigned int hw_base = cn==0 ? TGC_TABLE_CH0 : TGC_TABLE_CH1;
 
	table_fd[cn] = open("/dev/mem", O_RDWR | O_SYNC);
	if( !(table_fd[cn] > -1) ){
		printf("Can't open dev-mem.\n");
		return -1;
	}

	table_mapped_base[cn] = mmap( 
		0, 
		MAP_SIZE, 
		PROT_READ | PROT_WRITE, 
		MAP_SHARED, table_fd[cn], 
		(off_t)(hw_base & ~MAP_MASK) 
	);

	if (table_mapped_base[cn] == (void *) -1) {
		printf("Can't map the memory to user space.\n");
		return -1;
	}

	table_base[cn] = table_mapped_base[cn] + (hw_base & MAP_MASK);
	return (int)table_base[cn];
}

int tgc_table_close(int cn)
{
	if (munmap(table_mapped_base[cn], MAP_SIZE) == -1) {
		printf("Can't unmap memory from user space.\n");
		return -1;
	}else{
		close(table_fd[cn]);
		return 0;
	}
}

inline void tgc_table_set(int cn, int *pt, int length)
{
	memcpy((void*)table_base[cn], (void*)pt, sizeof(int)*length);
}
