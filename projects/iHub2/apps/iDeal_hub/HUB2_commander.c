//
//	"SET"
//	1. channel 0 enable 		-> reg0(4)
//	2. channel 1 enable 		-> reg0(5)
//	3. enable sowftware do 		-> reg0(2)
//	4. enable sowftware ready out 	-> reg0(3)
//	5. set do 			-> reg0(0)
//	6. set ready 			-> reg0(1)
//	7. seed ctp 			-> reg2, reg3, reg4
//	8. reset fpga 			-> reg0(31)
//	9. latch ctp and timestamp - reading ctp_x
//
//	"GET"
//	1. do in	-> reg1(0)
//	2. ready in	-> reg1(1)
//	3. master	-> reg1(8)
//	4. backplane    -> reg1(9)
//	5. slot id      -> reg1(15..12)
//	6. ctp and timestamp -> reg2, reg3, reg4 and reg5, reg6
//	7. channel ready -> reg0(5..4) 
//      8. VERSION reg7
//
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/mman.h>
#include "HUB2_commander.h"

#define HUB2_COMMANDER_BASE				0x43c40000
#define HUB2_COMMANDER_MAP_SIZE 		4096UL
#define HUB2_COMMANDER_MAP_MASK 		(HUB2_COMMANDER_MAP_SIZE - 1)

#define HUB2_COMMANDER_CTRL_OFFSET		(4*0)
#define HUB2_COMMANDER_STATUS_OFFSET	(4*1)
#define HUB2_COMMANDER_CTP_X_OFFSET		(4*2)
#define HUB2_COMMANDER_CTP_Y_OFFSET		(4*3)
#define HUB2_COMMANDER_CTP_Z_OFFSET		(4*4)
#define HUB2_COMMANDER_TS_LO_OFFSET		(4*5)
#define HUB2_COMMANDER_TS_HI_OFFSET		(4*6)
#define HUB2_COMMANDER_VERSION_OFFSET	(4*7)

static int hub2_commander_fd = -1;
static void* mapped_base = NULL;
static void* base = NULL;

int hub2_mngr_open()
{
	hub2_commander_fd = open("/dev/mem", O_RDWR | O_SYNC);
	if( !(hub2_commander_fd > -1) ){
		printf("Can't open dev-mem.\n");
		return -1;
	}

	mapped_base = mmap( 
		0, 
		HUB2_COMMANDER_MAP_SIZE, 
		PROT_READ | PROT_WRITE, 
		MAP_SHARED, hub2_commander_fd,
		(off_t)(HUB2_COMMANDER_BASE & ~HUB2_COMMANDER_MAP_MASK) 
	);

	if (mapped_base == (void *) -1) {
		printf("Can't map the memory to user space.\n");
		return -1;
	}

	base = mapped_base + (HUB2_COMMANDER_BASE & HUB2_COMMANDER_MAP_MASK);

	return (int)base;

}

int hub2_mngr_close()
{
	if (munmap(mapped_base, HUB2_COMMANDER_MAP_SIZE) == -1) {
		printf("Can't unmap memory from user space.\n");
		return -1;
	}else{
		close(hub2_commander_fd);
		return 0;
	}
}


static inline void hub2_mngr_set_bit(int shift, int value)
{
	unsigned int r;
	// TODO: crit. sect. ?
	r = *((volatile unsigned int *) (base + HUB2_COMMANDER_CTRL_OFFSET));
	r = r & (~(0x00000001<<shift));
	r = r | ((value?0x00000001:0)<<shift);
	*((volatile unsigned int*) (base + HUB2_COMMANDER_CTRL_OFFSET)) = r;
}
//static inline int hub2_ctrl_get(int parameter_id){ return 0; }
//static inline int hub2_ctrl_get_last_errno(void){ return 0; }

void hub2_mngr_set_do(int value)
{
	//unsigned int r;
	hub2_mngr_set_bit(0, value);
	//r = *((volatile unsigned int *) (base + HUB2_COMMANDER_CTRL_OFFSET));
}
void hub2_ctrl_set_ready(int value)
{
	hub2_mngr_set_bit(1, value);
}
void hub2_mngr_set_swdo(int enable)
{
	hub2_mngr_set_bit(2, enable);
}
void hub2_mngr_set_swready(int enable)
{
	hub2_mngr_set_bit(3, enable);
}
void hub2_mngr_set_ch_en(int channel, int enable)
{
	assert( (channel==0)||(channel==1) );
	hub2_mngr_set_bit(4+channel, enable);
}
void hub2_mngr_set_fpga_reset(int value)
{
	hub2_mngr_set_bit(31, value);
}
void hub2_mngr_set_ctp(ctp_t *ctp)
{
	*((volatile unsigned int*) (base + HUB2_COMMANDER_CTP_X_OFFSET)) = ctp->x;
	*((volatile unsigned int*) (base + HUB2_COMMANDER_CTP_Y_OFFSET)) = ctp->y;
	*((volatile unsigned int*) (base + HUB2_COMMANDER_CTP_Z_OFFSET)) = ctp->z;
	*((volatile unsigned int*) (base + HUB2_COMMANDER_TS_LO_OFFSET)) = ctp->ts_lo;
	*((volatile unsigned int*) (base + HUB2_COMMANDER_TS_HI_OFFSET)) = ctp->ts_hi;
}

inline int hub2_mngr_get_do_in()
{
	unsigned int r;
	r = *((volatile unsigned int *) (base + HUB2_COMMANDER_STATUS_OFFSET));
	return (r & 0x00000001) ? 1 : 0;
}
inline int hub2_mngr_get_ready_in()
{
	unsigned int r;
	r = *((volatile unsigned int *) (base + HUB2_COMMANDER_STATUS_OFFSET));
	return (r & 0x00000002) ? 1 : 0;
}
inline int hub2_mngr_get_ch_ready()
{
	unsigned int r;
	r = *((volatile unsigned int *) (base + HUB2_COMMANDER_STATUS_OFFSET));
	return ((r>>4) & 0x00000003);
}
inline int hub2_mngr_get_slot_id()
{
	unsigned int r;
	r = *((volatile unsigned int *) (base + HUB2_COMMANDER_STATUS_OFFSET));
	return ((r>>12))&0x0000000F;
}
inline int hub2_mngr_is_master()
{
	unsigned int r;
	r = *((volatile unsigned int *) (base + HUB2_COMMANDER_STATUS_OFFSET));
	return (r & 0x00000100) ? 1 : 0;
}
inline int hub2_mngr_is_backplane()
{
	unsigned int r;
	r = *((volatile unsigned int *) (base + HUB2_COMMANDER_STATUS_OFFSET));
	return (r & 0x00000200) ? 1 : 0;
}
void hub2_mngr_get_ctp(ctp_t *ctp)
{
	ctp->x = *((volatile unsigned int *) (base + HUB2_COMMANDER_CTP_X_OFFSET));
	ctp->y = *((volatile unsigned int *) (base + HUB2_COMMANDER_CTP_Y_OFFSET));
	ctp->z = *((volatile unsigned int *) (base + HUB2_COMMANDER_CTP_Z_OFFSET));
	ctp->ts_lo = *((volatile unsigned int *) (base + HUB2_COMMANDER_TS_LO_OFFSET));
	ctp->ts_hi = *((volatile unsigned int *) (base + HUB2_COMMANDER_TS_HI_OFFSET));
}
inline unsigned int hub2_mngr_get_version(void)
{
	register unsigned int r;
	r = *((volatile unsigned int *) (base + HUB2_COMMANDER_VERSION_OFFSET));
	return r;
}

void hub2_mngr_do()
{
	hub2_mngr_set_bit(2, 1);
	hub2_mngr_set_bit(2, 0);
}

void hub2_mngr_fpga_reset()
{
	/*
	 * for hub2_commander ip core version 16120903: default reset is active
	 * so it is better to call this function "release from reset"
	 */
	hub2_mngr_set_bit(31, 0);
}
