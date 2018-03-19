//
//	SET ADC
//	1. RX_DELAY	reg0(27 downto 0)
//	2. SAMPLES      reg1(17 downto 0)
//	3. CTRL         reg2
//	4. STATUS       reg3
//
//	SET TGC

//	1. CTRL		reg4(1..0) 0-parameters ready, 1-?
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
#if     !defined(_HUB2_CTRL_H_)
#define _HUB2_CTRL_H_

int tgc_ctrl_open(int cn);
int tgc_ctrl_close(int cn);
inline void tgc_run(int cn);// set reg4(0) i.e. set PARAMETERS_READY
inline void tgc_sw_do(int cn);// set reg4(1) i.e. set CHANNEL_SW_DO
inline void tgc_set_active(int cn, int active);
inline void tgc_set_delay(int cn, int tx_delay);
inline void tgc_set_length(int cn, int tgc_table_length); 
inline void tgc_set_ratio(int cn, int ratio);// adc ticks per one tgc tick. must be even. It is constantly 10 by specification.
inline void tgc_set_reg(int cn, short addr, short value);// using reg9..reg13

int  tgc_dbg_reg_wr(int cn, short addr, short value);// using direct access via reg14

int  tgc_table_open(int cn);
int  tgc_table_close(int cn);
inline void tgc_table_set(int cn, int *pt, int length);

inline void adc_set_delay(int cn, int rx_delay);
inline void adc_set_length(int cn, int samples_number);

#endif
