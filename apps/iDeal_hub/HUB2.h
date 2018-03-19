/*
	2016.08.16
*/

#if !defined(_HUB2_H_)
#define _HUB2_H_ 

#define IDEAL_HUB

#ifdef WIN32
#define INET6_ADDRSTRLEN (48)
#endif

#define CHANNELS (2)

#define DBG_HOST_IP				("192.168.100.15"/*"127.0.0.1"*/)
#define DBG_HOST_UDP_PORT		((unsigned short)8888)
#define DBG_HOST_TCP_PORT		((unsigned short)30000)

#define DEFAULT_HOST_TCP_PORT	((unsigned short)30000)

#define MEASURE_MODE_SINGLE		(0)
#define MEASURE_MODE_CONTINUES	(1)

#define PRAM_SIZE		        ((int)(8*1024))/* Number of mPSETs in the mPRAM table */
#define PSET_STATIC_SIZE        ((int)64)
#define PSET_DYNAMIC_SIZE       ((int)256)
#define STRAM_SIZE		        ((int)64)

#define SHOT_BUFFER_SIZE		(1024 * 128) /* [unsigned short] */

#define IDENTIFY_MSG	("i-Deal HUB2:\r\n channel %2d\r\n backplane ID %2d\r\n master %c\n\r")
#define IDENTIFY_SIZE (sizeof(IDENTIFY_MSG) - 3) /* "3" is for "%" */

#define DEFAULT_MCLK_FREQ_MHZ (250)
#define DEFAULT_MCLK_FREQ_HZ  (DEFAULT_MCLK_FREQ_MHZ*1000000)

typedef struct{
	unsigned int Static[PSET_STATIC_SIZE];
	unsigned int Dynamic[PSET_DYNAMIC_SIZE];
} mPSET_t;// mPSET size is 1280 Bytes. ( 64 + 256 ) * 4 = 320 * 4 = 1280.

typedef struct{
	mPSET_t mPSET[PRAM_SIZE];
} mPRAM_t;

typedef struct{
	int mPSETindex[PRAM_SIZE]; /* mPSET index */
} mSTBL_t;

typedef struct{
	mSTBL_t mSTBL[STRAM_SIZE];
} mSRAM_t; 




typedef struct{
	unsigned short HeaderID;// 1. magic constant 0x0010
	unsigned short HeaderSz;// 2. header size: number of bytes
	unsigned short mPSET;	// 3. mPSET index
	unsigned short mSTBL;	// 4. mSTBL index
	unsigned short data_id_lo;// 5. data tag
	unsigned short data_id_hi;// 6.
	unsigned short CTP1_lo;	// 7.
	unsigned short CTP1_hi;	// 8.
	unsigned short CTP2_lo;	// 9.
	unsigned short CTP2_hi;	// 10.
	unsigned short CTP3_lo;	// 11.
	unsigned short CTP3_hi;	// 12.
	unsigned short FSI_lo;	// 13. first sample index
	unsigned short FSI_hi;	// 14.
	unsigned short data_sz_lo;// 15. Data size (in bytes)
	unsigned short data_sz_hi;// 16.
}shot_header_t;

typedef struct{
	unsigned short* pHeader;
	unsigned short* pData;
} shot_t;

typedef enum{
	CMD_FAILURE = -1,			// this is not command, this is indicator of failure
	CMD_NOP,					// do nothing
	CMD_PING,					// 
	CMD_IDENTIFY,				// Ask channel "who are you"
	CMD_REBOOT,					// Reset HUB2 board: reboot everithing.
	CMD_RESET,					// Reset channel: fpga, adc, soft. Set channel to IDLE state.
	CMD_START_MEASURE,			// 
	CMD_STOP_MEASURE,			//
	CMD_GET_STATE,				// Get state of some HUB2's channel
	CMD_GET_SHOTS_COUNT,		// Get nuber of shots in the HUB2's memory
	CMD_GET_SHOTS,				// Parameters: Start_Index, Number_of_shorts
	CMD_SET_MPSET,				// Parameters: mPSET_Index, datasize_in_bytes
	CMD_GET_MPSET,				// Parameters: mPSET_Index, datasize_in_bytes
	CMD_SET_STBL,				// Parameters: mSTBL_Index, datasize_in_bytes
	CMD_GET_STBL,				// Parameters: mSTBL_Index, datasize_in_bytes
	CMD_SET_CTP,				// write CTP coordinats
	CMD_GET_CTP,				// read  CTP coordinats
	CMD_SET_REG_MCFG,			// Measure configuraton regiser: measure mode (0- single shot,1- continuous ); overflow mode (0,1,2); CTP mode (0- extern, 1- emulator); shots sending mode (0- send by readyness, 1- send by request); ADC mode (0- real, 1- emulator)
	CMD_GET_REG_MCFG,			// 
	CMD_SET_MSR_MODE,			// Set measure mode: 0 - single shot, 1 - continuous.
	CMD_GET_MSR_MODE,			// Get measure mode.
	CMD_SET_OVF_MODE,			// Set overflow ADC mode: 0 - 14-bit Optus default, 1 - 16-bit 2's complement, 2 - 16-bit unsigned.
	CMD_GET_OVF_MODE,			// Get overflow ADC mode.
	CMD_SET_CTP_MODE,			// Set CTP mode: 0 - external, 1 - internal (emulator).
	CMD_GET_CTP_MODE,			// Get CTP mode.
	CMD_SET_SND_MODE,			// Set shot data sending mode: 0 - auto, 1 - by request from server.
	CMD_GET_SND_MODE,			// Get shot data sending mode.
	CMD_SET_ADC_MODE,			// Set ADC mode: 0 - ADC, 1 - emulation.
	CMD_GET_ADC_MODE,			// Get ADC mode.
	CMD_SET_REG_STBL,			// Write rSTBL.  Parameters: value (32 bit ?)
	CMD_GET_REG_STBL,			// Read  rSTBL.  Parameters: value (32 bit ?)
	CMD_SET_REG_FPGA,			// Write 32-bit FPGA register. Parameters: value (32 bit ?), address (32 bit)
	CMD_GET_REG_FPGA,			// Read  32-bit FPGA register. Parameters: value (32 bit ?), address (32 bit)
	CMD_SET_REG_ADC,			// Write 32-bit ADC register. Parameters: value (32 bit ?), address (32 bit ?)
	CMD_GET_REG_ADC,			// Read  32-bit ADC register. Parameters: value (32 bit ?), address (32 bit ?)
	CMD_CONNECT_LOSS,			// send by channel to reply queue in case of connection lost
	CMD_DISCONNECT,				// server off, stay in idle
	CMD_DIE						// on delete channel
} command_t;

typedef enum{
	ACK_OK = 0,		// OK, no errors
	ACK_NOTSUPP,	// not supported
	ACK_NOTIMPL,	// not implemented
	ACK_ERROR		// where and what?
} acknolage_t;
//////////////////////////// UDP Listener ///////////////////////////
#define MAXBUF (508)

typedef enum{
	CMD_IDLE,
	CMD_LISTEN,
	CMD_EXIT
} listener_cmd_t;

typedef struct{
	int port;// to listener thread
	int command; // to listener thread
	int state;
	int msg_ok;//1 if message was receiver without error
	int msg_count;// number of received messages
	int msg_size;// in bytes
	char msg_addr[INET6_ADDRSTRLEN];// address of message sender
	char msg[1024];
} listener_state_t;
//////////// end UDP listener related types and defs /////////////////
#endif
