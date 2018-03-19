/*
	2016.08.16
	channel data strustures and methods
*/
#if     !defined(_HUB2_CHANNEL_H_)
#define _HUB2_CHANNEL_H_ 

#include "HUB2.h"
#include "HUB2_board_info.h"

#define FILEPATH "iDeal"

#define OK	0
#define NET_SOFTERROR -1
#define NET_HARDERROR -2

#define HUB2_CFG_IN ((unsigned short) 0x0011)
#define HUB2_CTL_IN ((unsigned short) 0x0012)

typedef struct{
	unsigned short sample[SHOT_BUFFER_SIZE];
} shot_data_t;

typedef enum{
	CHANNEL_STATE_ABSENT = 0,
	CHANNEL_STATE_CONNECT,
	CHANNEL_STATE_IDLE,
	CHANNEL_STATE_MEASURE,
	CHANNEL_STATE_CONFIG,
	CHANNEL_STATE_EXIT,
	CHANNEL_STATE_ERROR
} HUB2_channel_state_t;

typedef struct{
	int number;						// channel number
	int state;
	int adc;						// file descriptor of adc driver
	board_info_t* board_info;				// carrier board and system info
#if defined(USE_IIO)
	struct iio_device* dev;
	struct iio_buffer* buf;
#endif
	int socket;						//TCP socket for data/command/settings
    listener_state_t *listener;     //
	struct sockaddr_in serv_addr;	// server address and port
	unsigned short *rxbuf;			// for command and settings
	int rxbuf_size;					// in bytes
	unsigned short *txbuf;			// for command and settings
	int txbuf_size;					// in bytes
	int hdr_id;						// latest received net header id
	int hdr_size;					// size of command header (in bytes)
	command_t cmd_id;				// just received command id
	int cmd_size;					// size of command data (in bytes)
	int is_shot;					// 
	shot_t	shot;					//
	int shot_count;					// counts transmitted shots
	int emulate_adc;
	void* chmtx;					// pointer to common channels mutex
	int shot_mode;					// 0 - single, 1 - continuous
	int shot_send_mode;				// 0 - simple, 1 - by request
	int ovf_mode;					// overflow mode (0,1,2)
	int ctp_mode;					// 0- extern, 1- emulator
	int data_end;					// 0 - little, 1 - big
	mPRAM_t* mPRAM;
	mSRAM_t* mSRAM;
	unsigned int current_adc_frq;	// currnet adc frequency
	unsigned int current_samples;	// currnet shot samples number
	int current_stbl;				// crrent shot table
	int current_pset;				// current record in the current shot table
	//shot_parameters_t shot_params;
} HUB2_channel_t;

/* */
HUB2_channel_t* hub2_channel_create(int channel_number);
void hub2_channel_destroy(HUB2_channel_t* c);

/*	open hardware (adc driver, tgc driver) */
int hub2_channel_open(HUB2_channel_t* c, void* ptr);
int hub2_channel_close(HUB2_channel_t* c);

/* set pointers to mPRAM and mSRAM */
void hub2_channel_init(HUB2_channel_t* c, void* ptr);

/* set server address and port */
int hub2_channel_netcfg(HUB2_channel_t* c, listener_state_t *a);

/* create connection with host (server) */
int hub2_channel_connect(HUB2_channel_t* c);
int hub2_channel_disconnect(HUB2_channel_t* c);

/* main channel thread */
void* hub2_channel_thread(void *p);

/* hub2_config_read() - ÷òåíèå íàñòðîåê èç ÏÇÓ */
//int hub2_channel_config_read();

/* hub2_config_read() - çàïèñü íàñòðîåê â ÏÇÓ */
//int hub2_channel_config_save();

/* hub2_config_apply() - óòàíîâêà íàñòðîåê â ñèñòåìó */
//int hub2_channel_config_apply();

/* hub2_is_master() return 1 if board is master */
//int hub2_channel_is_master();

/* hub2_is_backplane() return 1 if board is on backplane */
//int hub2_channel_is_backplane();

/* hub2_get_slot_index() reurn backplane slot index(number) */
//int  hub2_channel_get_slot_index();
#endif
