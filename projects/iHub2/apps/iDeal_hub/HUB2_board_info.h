#if     !defined(_HUB2_BOARDINFO_H_)
#define _HUB2_BOARDINFO_H_

typedef struct{
	int channels;
	int slot_id;
	int master;
	int backplane;
	int clk_freq_hz;
}board_info_t;

#endif
