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
//	2. ready in	-> reg0(1)
//	3. master	-> reg0(8)
//	4. backplane    -> reg0(9)
//	5. slot id      -> reg0(15..12)
//	6. ctp and timestamp -> reg2, reg3, reg4 and reg5, reg6
//	7. channel ready -> reg0(5..4) 
//      8. VERSION reg7
//
#if     !defined(_HUB2_COMMANDER_H_)
#define _HUB2_COMMANDER_H_

typedef struct{
	unsigned int x;
	unsigned int y;
	unsigned int z;
	unsigned int ts_lo;// time stamp's lo
	unsigned int ts_hi;// time stamp's hi
}ctp_t;

int hub2_mngr_open();
int hub2_mngr_close();

void hub2_mngr_set_ch_en(int channel, int enable);// enable channel
void hub2_mngr_set_swdo(int enable);
void hub2_mngr_set_swready(int enable);
void hub2_mngr_set_do(int value);
void hub2_mngr_set_ready(int value);
void hub2_mngr_set_ctp(ctp_t *ctp);
void hub2_mngr_set_fpga_reset(int value);
void hub2_mngr_do();
void hub2_mngr_fpga_reset();

int hub2_mngr_get_do_in();
int hub2_mngr_get_ready_in();
int hub2_mngr_get_ch_ready();

int hub2_mngr_get_slot_id();
int hub2_mngr_is_master();
int hub2_mngr_is_backplane();
unsigned int hub2_mngr_get_version();

void hub2_mngr_get_ctp(ctp_t *ctp);

#endif
