/*
	2016.08.16
*/
#include <stdio.h>
#include <assert.h>

#ifdef WIN32
#include "stdafx.h"
#include <winsock2.h>
#include <conio.h>
#else

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include <iio.h>

//#include <netdb.h>
//#include <netinet/tcp.h>
//#include <sys/time.h>
//#include <sys/select.h>
#endif

#include "HUB2.h"
#include "HUB2_channel.h"

#include "HUB2_ctrl.h"
#include "clk_idt5p49v6914a.h"

#define RX_BUFF_SIZE (1024*32) /* network buffer [unsigned short] */
#define RX_BUFF_SIZE_IN_BYTES	(RX_BUFF_SIZE * sizeof(unsigned short))

#define TX_BUFF_SIZE (1024*32) /* network buffer [unsigned short] */
#define TX_BUFF_SIZE_IN_BYTES	(TX_BUFF_SIZE * sizeof(unsigned short))


static HUB2_channel_t ch[CHANNELS]; /* HUB2 support only 2 channels */

static mPRAM_t mPRAM[CHANNELS];
static mSRAM_t mSRAM[CHANNELS];

static shot_header_t sh[CHANNELS];/* for adc emulation */
static shot_data_t sd[CHANNELS]; /* for adc emulation */

/* reads 'count' bytes from a socket  */
int net_read(int fd, char *buf, size_t count)
{
#ifdef WIN32
	register size_t r;
	register size_t nleft = count;

	while (nleft > 0) {
		r = recv(fd, buf, nleft, 0);
		if (r < 0) {
			int err = WSAGetLastError();
			if (err == WSAEINTR || errno == WSAEWOULDBLOCK)
				break;
			else
				return NET_HARDERROR;
		}
		else if (r == 0)
			break;

		nleft -= r;
		buf += r;
	}
	return count - nleft;
#else
	register ssize_t r;
	register size_t nleft = count;

	while (nleft > 0) {
		r = read(fd, buf, nleft);
		if (r < 0) {
			if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
				break;
			else
				return NET_HARDERROR;
		} else if (r == 0)
			break;

		nleft -= r;
		buf += r;
	}
	return count - nleft;
#endif
}

int net_write(int fd, const char *buf, size_t count)
{
#ifdef WIN32
	register size_t r;
	register size_t nleft = count;

	while (nleft > 0) {
		r = send(fd, buf, nleft, 0);
		if (r < 0) {
			int err = WSAGetLastError();
			switch (err) {
			case WSAEINTR:
			case WSAEWOULDBLOCK:
				return count - nleft;

			case WSAEFAULT:
				return NET_SOFTERROR;

			default:
				return NET_HARDERROR;
			}
		}
		else if (r == 0)
			return NET_SOFTERROR;
		nleft -= r;
		buf += r;
	}
	return count;
#else
	register ssize_t r;
	register size_t nleft = count;

	while (nleft > 0) {
		r = write(fd, buf, nleft);
		if (r < 0) {
			switch (errno) {
			case EINTR:
			case EAGAIN:
#if (EAGAIN != EWOULDBLOCK)
			case EWOULDBLOCK:
#endif
				return count - nleft;

			case ENOBUFS:
				return NET_SOFTERROR;

			default:
				return NET_HARDERROR;
			}
		} else if (r == 0)
			return NET_SOFTERROR;
		nleft -= r;
		buf += r;
	}
	return count;
#endif
}

/* non-blocking read */
int have_command(HUB2_channel_t *c)
{
	if (-1 < c->socket){
		unsigned long have;
		int rv;
    #ifdef WIN32
		rv = ioctlsocket(c->socket, FIONREAD, (unsigned long*)&have);
    #else
        rv = ioctl(c->socket, FIONREAD, (unsigned long*)&have);
    #endif
		if (0 == rv){
			if (3 < have)
				return have;
		}
	}
	return 0;
}
/* blocking read */
int receive_command(HUB2_channel_t *c)
{
	int count, data_size;

	assert(NULL != c);
	// TODO: do we have to check c->socket fd?

	// receive network header_id and command_header_size
	data_size = sizeof(unsigned short) * 2;
	count = net_read(c->socket, (char*)&c->rxbuf[0], data_size);
	if (count == 0){
		printf("CH#%d: Server close connection. Exit...\n", c->number);
		c->hdr_id = -1;
		c->hdr_size = -1;
		return -1;
	}
	if (count != data_size){
		perror("Error in receive_command(): header reading");
		c->hdr_id = -1; 
		c->hdr_size = -1;
		return -1;// reconnect?
	}
	c->rxbuf[0] = ntohs(c->rxbuf[0]);// net id
	c->rxbuf[1] = ntohs(c->rxbuf[1]);// command_header_size
	c->hdr_id = (int)c->rxbuf[0];
	c->hdr_size = (int)c->rxbuf[1];
	printf("CH#%d: Net header 0x%04x size %d\n", c->number, c->hdr_id, c->hdr_size);

	// receive command header and command_data_size
	data_size = c->hdr_size;
	assert(0 == data_size%sizeof(unsigned short));
	count = net_read(c->socket, (char*)&c->rxbuf[2], data_size);
	if (count == 0){
		printf("Server close connection. Exit...\n");
		c->hdr_id = -1;
		c->hdr_size = -1;
		c->cmd_id = CMD_FAILURE;
		c->cmd_size = -1;
		return -1;
	}
	assert(count == data_size);
	if (count != data_size){
		perror("Error in receive_command(): command reading");
		c->cmd_id = CMD_FAILURE;
		c->cmd_size = -1;
		return -1;// reconnect?
	}
	c->rxbuf[2] = ntohs(c->rxbuf[2]);// command id
	c->rxbuf[3] = ntohs(c->rxbuf[3]);// command data size
	c->cmd_id = (command_t)c->rxbuf[2];
	c->cmd_size = (int)c->rxbuf[3]; // command data size
	printf("    command 0x%04x, data size %d\n", c->rxbuf[2], c->rxbuf[3]);

	// receive (if it is) data
	data_size = c->cmd_size;
	if (0 == data_size) return OK;
	assert(0 == data_size%sizeof(unsigned short));
	assert(c->rxbuf_size >= (data_size + 4 * (int)sizeof(short)));
	if (c->rxbuf_size < (data_size + 4 * (int)sizeof(short))){
		printf("ERROR: unsupported data size of command. Reconnecting...\n");
		return -2;// TODO: UNSUPPORTED_DATA_SIZE !!! We must read or reconnect!!!
	}
	count = net_read(c->socket, (char*)&c->rxbuf[2 + (c->hdr_size/sizeof(short))], data_size);
	if (count == 0){
		printf("Server close connection. Exit...\n");
		c->hdr_id = -1;
		c->hdr_size = -1;
		return -1;
	}
	assert(count == data_size);
	if (count != data_size){
		perror("Error in receive_command(): command data reading");
		c->cmd_size = -1;
		return -1;// reconnect?
	}
	return OK;
}

int echo(int sock, void* tx_buf, int tx_buf_size)
{
	unsigned short *net_header = (unsigned short*)tx_buf;
	int data_size, tx_size;

	data_size = 4 * sizeof(unsigned short);
	assert(data_size <= tx_buf_size);
	net_header[0] = htons(net_header[0]);
	net_header[1] = htons(net_header[1]);
	net_header[2] = htons(net_header[2]);
	net_header[3] = htons(net_header[3]);
	tx_size = net_write(sock, (char*)&net_header[0], data_size);
	assert(tx_size == data_size);
	printf("    echo reply has been sended, size %d\n", tx_size);
	return tx_size;
}

void init_shot(HUB2_channel_t *c);
static void process_command_set_mpset(HUB2_channel_t *c)
{
	int N;
	//we did this earlie
	//memcpy(c->txbuf, c->rxbuf, sizeof(unsigned short)* 4);
	switch (c->hdr_size){
	case 8:
		assert(c->cmd_size == sizeof(mPSET_t));
		memcpy(&N, &(c->rxbuf[4]), sizeof(int));
		// TODO: check endiannes and convert if need
		if (N<PRAM_SIZE && N > -1){
			memcpy(&(mPRAM[c->number].mPSET[N]), &(c->rxbuf[6]), sizeof(mPSET_t));
			c->txbuf[1] = 0x0004;//sizeof(short) * 2
			c->txbuf[3] = 0;
			echo(c->socket, c->txbuf, TX_BUFF_SIZE_IN_BYTES);
		}
		else{
			// TODO: implement
			assert(0);
		}
		break;
	default:
		// TODO: echo with "unsupported size/type" or something like this
		printf("CH#%d: Error unsupported CFG command size\n", c->number);
		assert(0);
		break;
	}
}
static void process_command_set_mstbl(HUB2_channel_t *c)
{
	int N;
	//we did this earlie
	//memcpy(c->txbuf, c->rxbuf, sizeof(unsigned short)* 4);
	switch (c->hdr_size){
	case 8:
		assert(c->cmd_size == sizeof(mSTBL_t));
		memcpy(&N, &(c->rxbuf[4]), sizeof(int));
		//TODO: check endiannes and convert if need
		if (N<PRAM_SIZE && N > -1){
			memcpy(&(mSRAM[c->number].mSTBL[N]), &(c->rxbuf[6]), sizeof(mSTBL_t));
			c->txbuf[1] = 0x0004;
			c->txbuf[3] = 0;
			echo(c->socket, c->txbuf, TX_BUFF_SIZE_IN_BYTES);
		}
		else{
			// TODO: implement
			assert(0);
		}
		break;
	default:
		// TODO: echo with "unsupported size/type" or something like this
		printf("CH#%d: Error unsupported CFG command size\n", c->number);
		assert(0);
		break;
	}
}
void process_command(HUB2_channel_t *c)
{
	int data_size, tx_size;

	if (-1 != c->hdr_id){
		switch (c->hdr_id){
		case HUB2_CFG_IN:
			memcpy(c->txbuf, c->rxbuf, sizeof(unsigned short)* 4);
			switch(c->cmd_id){
			case CMD_SET_MPSET:
				if (c->state != CHANNEL_STATE_MEASURE){
					process_command_set_mpset(c);
				}
				else{
					assert(0);//TODO: echo (reply) with UNABLE_BECAUSE_MEASURE
				}
				break;
			case CMD_GET_MPSET:
				assert(0);/// TODO: implement
				break;
			case CMD_SET_STBL:
				if(c->state != CHANNEL_STATE_MEASURE){
					process_command_set_mstbl(c);
				}else{
					assert(0);///TODO: reply with UNABLE_BECAUSE_MEASURE
				}
				break;
			case CMD_GET_STBL:
				assert(0);/// TODO: implement
				break;
			case CMD_SET_MSR_MODE:
				if (c->state != CHANNEL_STATE_MEASURE){
					if (c->hdr_size == sizeof(short)* 2 + sizeof(int)){
						int mode;
						memcpy(&mode, &c->rxbuf[4], sizeof(int));
						mode = ntohl(mode);
						assert(mode > -1);
						c->shot_mode = mode;
						c->txbuf[1] = 4;
						echo(c->socket, c->txbuf, TX_BUFF_SIZE_IN_BYTES);
						printf("CH#%d:    SHOT MODE %d\n", c->number, mode);
					}
					else{
						assert(0);// TODO: reply with error
					}
				}
				else{
					assert(0);///TODO: reply with UNABLE_BECAUSE_MEASURE
				}
				break;
			case CMD_SET_ADC_MODE:
				if (c->state != CHANNEL_STATE_MEASURE){
					if (c->hdr_size == sizeof(short)* 2 + sizeof(int)){
						int mode;
						memcpy(&mode, &c->rxbuf[4], sizeof(int));
						mode = ntohl(mode);
						assert(mode > -1);
						c->emulate_adc = mode;
						c->txbuf[1] = 4;
						echo(c->socket, c->txbuf, TX_BUFF_SIZE_IN_BYTES);
						printf("CH#%d:    ADC EMULATION %d\n", c->number, mode);
					}
					else{
						assert(0);// TODO: reply with error
					}
				}
				else{
					assert(0);///TODO: reply with UNABLE_BECAUSE_MEASURE
				}
				break;
			default:
				assert(0);/// TODO: implement
				break;
			}
			break;
		case HUB2_CTL_IN:
			memcpy(c->txbuf, c->rxbuf, sizeof(unsigned short)* 4);
			switch (c->cmd_id){
			case CMD_IDENTIFY:
				c->txbuf[0] = htons(0x0012);
				c->txbuf[1] = htons(4);
				c->txbuf[2] = htons(CMD_IDENTIFY);
        #ifdef WIN32
				sprintf_s((char*)&c->txbuf[4], 256, IDENTIFY_MSG, c->number, 0, 'y');
        #else
                sprintf((char*)&c->txbuf[4], IDENTIFY_MSG, c->number, 0, 'y');
        #endif
				data_size = strlen((char*)&c->txbuf[4]);
				c->txbuf[3] = htons(data_size);
				data_size += 8;
				tx_size = net_write(c->socket, (char*)&c->txbuf[0], data_size);
				printf("CH#%d:    reply has been sended, size %d\n", c->number, tx_size);
				assert(tx_size == data_size);
				break;
			case CMD_DISCONNECT:
				echo(c->socket, c->txbuf, TX_BUFF_SIZE_IN_BYTES);
				printf("CH#%d: DISCONNECT\n", c->number);
				hub2_channel_disconnect(c);
				c->state = CHANNEL_STATE_CONNECT;
				break;
			case CMD_DIE:
				echo(c->socket, c->txbuf, TX_BUFF_SIZE_IN_BYTES);
				printf("CH#%d:    Final Goodbye has been sended\n", c->number);
				c->state = CHANNEL_STATE_EXIT;
				break;
			case CMD_START_MEASURE:
				printf("CH#%d:    CMD_START_MEASURE\n", c->number);
				echo(c->socket, c->txbuf, TX_BUFF_SIZE_IN_BYTES);
				init_shot(c);
				c->shot_count = 0;
				c->state = CHANNEL_STATE_MEASURE;
				break;
			case CMD_STOP_MEASURE:
				printf("CH#%d:    CMD_STOP_MEASURE\n", c->number);
				echo(c->socket, c->txbuf, TX_BUFF_SIZE_IN_BYTES);
				c->state = CHANNEL_STATE_IDLE;
				break;
			default:
				echo(c->socket, c->txbuf, TX_BUFF_SIZE_IN_BYTES);
				break;
			}
			break;
		default:
			printf("CH#%d: Error unsupported header id\n", c->number);
			break;
		}
	}
}

static void process_exit(HUB2_channel_t *c)
{
	// TODO: ...
	return;
}

static int have_shot(HUB2_channel_t *c)
{
	int shots = 0;
	if(c->emulate_adc){
		shots = 1;
	}else{
	#if defined(USE_IIO)
		shots = 1;// TODO: is it always true for iio?
	#else
		assert(0);
	#endif
	}
	return shots;
}
int get_shot(HUB2_channel_t *c)
{
	int rv = OK;
	if (c->emulate_adc){
		c->shot.pHeader = (unsigned short*)&sh[c->number];
		c->shot.pData = (unsigned short*)&sd[c->number].sample[0];
		c->is_shot = 1;
	}else{
#if defined(USE_IIO)
		assert(c->buf);

		rv = iio_buffer_refill(c->buf);
		if(0 < rv){
			c->shot.pHeader = (unsigned short*)&sh[c->number];//header filled in the init_shot
			c->shot.pData = (unsigned short*)iio_buffer_start(c->buf);
			c->is_shot = 1;
			rv = OK;
		}else{
			printf("\n  CH#%d iio_buffer_refill() ERROR: %x (%d)\n\n", c->number, rv,rv);
			c->is_shot = 0;
			//assert(rv != -16);// busy
			//assert(rv != -110);// timeout
			rv=-1;
		}
#else
		assert(0);		
#endif
	}
	return rv;
}
int send_shot(HUB2_channel_t *c)
{
	unsigned short sh[sizeof(shot_header_t) / sizeof(unsigned short)];
	int tx_size, data_size;

	if ( 1==c->is_shot && NULL!=c->shot.pHeader && NULL!=c->shot.pData)
	{
		shot_header_t* shot = (shot_header_t*)c->shot.pHeader;
		short* data = (short*)c->shot.pData;
		int shot_data_size = (int)shot->data_sz_lo + (((int)shot->data_sz_hi) << 16);
		
		sh[0] = htons(shot->HeaderID);
		sh[1] = htons(shot->HeaderSz);
		sh[2] = htons(shot->mPSET);
		sh[3] = htons(shot->mSTBL);
		sh[4] = htons(shot->data_id_lo);
		sh[5] = htons(shot->data_id_hi);
		sh[6] = htons(shot->CTP1_lo);
		sh[7] = htons(shot->CTP1_hi);
		sh[8] = htons(shot->CTP2_lo);
		sh[9] = htons(shot->CTP2_hi);
		sh[10] = htons(shot->CTP3_lo);
		sh[11] = htons(shot->CTP3_hi);
		sh[12] = htons(shot->FSI_lo);
		sh[13] = htons(shot->FSI_hi);
		sh[14] = htons(shot->data_sz_lo);
		sh[15] = htons(shot->data_sz_hi);

		data_size = sizeof(shot_header_t);
		tx_size = net_write(c->socket, (char*)sh, data_size);
		if(tx_size < data_size){
            return -1;
        }
		//printf("  send hdr %d .. ", data_size);
		data_size = shot_data_size;
		tx_size = net_write(c->socket, (char*)data, data_size);
        if(tx_size < data_size){
            return -1;
        }
		//printf("send data %d\n", data_size);
		c->is_shot = 0;
		c->shot_count++;
		return OK;
	}
	else{
		printf("send_shot() err\n");
		return -1;
	}

}
static int release_shot(HUB2_channel_t *c)
{
	int rv = OK;
	if (c->is_shot > -1){
#if defined(USE_IIO)
#else
	assert(0);
#endif
		c->is_shot = -1;
	}
	return rv;
}
void init_shot(HUB2_channel_t *c)
{
	c->current_pset = 0;
	c->current_stbl = 0;
	c->current_samples = 0;

	if (!c->emulate_adc){
		;
	}else{
		int i;
		int channel = c->number;
		sh[channel].HeaderID = 0x0010;
		sh[channel].HeaderSz = sizeof(shot_header_t);
		sh[channel].mPSET = 1;
		sh[channel].mSTBL = 3;
		sh[channel].data_id_lo = 0;
		sh[channel].data_id_hi = 0;
		sh[channel].CTP1_lo = 1;
		sh[channel].CTP1_hi = 2;
		sh[channel].CTP2_lo = 3;
		sh[channel].CTP2_hi = 4;
		sh[channel].CTP3_lo = 5;
		sh[channel].CTP3_hi = 6;
		sh[channel].FSI_lo = 0;
		sh[channel].FSI_hi = 0;
		sh[channel].data_sz_lo = (unsigned short)(SHOT_BUFFER_SIZE & 0x0000FFFF);
		sh[channel].data_sz_hi = (unsigned short)( (SHOT_BUFFER_SIZE>>16) & 0x0000FFFF);

		for (i = 0; i < SHOT_BUFFER_SIZE; i++)
			sd[channel].sample[i] = (short)(i & 0x0000FFFF);
	}

	printf("CH#%d init_shot ok\n", c->number);
}
void set_shot(HUB2_channel_t *c)
{
	int si, ret;
	int channel = c->number;
	unsigned int id, sz, fq;

	//si = c->mSRAM->mSTBL[c->current_stbl].mPSETindex[c->current_pset];	// set index
	si = 0;
	id = c->mPRAM->mPSET[si].Static[6];									// set ID
	sz = c->mPRAM->mPSET[si].Static[3];									// samples (i.e shot size in samples)
	fq = c->mPRAM->mPSET[si].Static[2];									// ADC frequency

	assert(sz > 0);
	assert(sz < 0x00010001);// no more than 64*1024	

	sh[channel].HeaderID = 0x0010;
	sh[channel].HeaderSz = sizeof(shot_header_t);
	sh[channel].mPSET = c->current_pset;
	sh[channel].mSTBL = c->current_stbl;

	sh[channel].data_id_lo = (unsigned short)(id & 0x0000FFFF);
	sh[channel].data_id_hi = (unsigned short)((id>>16) & 0x0000FFFF);
	//	shot->data_id_lo = (unsigned short)(c->shot_count & 0x0000FFFF);
	//	shot->data_id_hi = (unsigned short)((c->shot_count >> 16) & 0x0000FFFF);

	sh[channel].CTP1_lo = 1;	// X
	sh[channel].CTP1_hi = 2;
	sh[channel].CTP2_lo = 3;	// Y
	sh[channel].CTP2_hi = 4;
	sh[channel].CTP3_lo = 5;	// Z
	sh[channel].CTP3_hi = 6;
	sh[channel].FSI_lo = 0;
	sh[channel].FSI_hi = 0;

	// multiplication is cause of slowness
	if (c->current_samples != sz){
		sh[channel].data_sz_lo = (unsigned short)((sz * sizeof(short)) & 0x0000FFFF);
		sh[channel].data_sz_hi = (unsigned short)(((sz * sizeof(short)) >> 16) & 0x0000FFFF);
		c->current_samples = sz;
		if (!c->emulate_adc){
			
			printf("CH#%d STBL=%d PSET=%d\n", c->number,c->current_stbl, c->current_pset);
			printf("CH#%d SAMPLES %d\n", c->number, sz/*c->shot_params.samples*/);
	#if defined(USE_IIO)			
			if(NULL != c->buf){
				iio_buffer_destroy(c->buf);
				c->buf = NULL;
			}

			//do not need it
			//ret = iio_device_set_kernel_buffers_count(c->dev, 8);
			//assert(0==ret);

			c->buf = iio_device_create_buffer(c->dev, (size_t)sz, false);
			assert(c->buf != NULL);
	#else
		assert(0);			
	#endif	
		
		}
	}

	if (c->current_adc_frq != fq){		
		printf("CH#%d STBL=%d PSET=%d\n", c->number,c->current_stbl, c->current_pset);
		printf("CH#%d ADC FRQ %d\n", c->number, fq/*c->shot_params.adc_frq*/);

		pthread_mutex_lock(c->chmtx);
		if( ((c->board_info->backplane == 1) && (c->board_info->master == 1)) || (c->board_info->backplane == 0) ){
			if( c->board_info->clk_freq_hz != fq ){
				if (OK == clk_open()){
					printf("CH#%d Current frq. %d. New frq. %d\n", c->number, c->current_adc_frq, fq);
					ret = clk_set_mhz(fq / 1000000);
					assert(0 == ret);
					c->board_info->clk_freq_hz = fq;
					clk_close();
					//TODO: do we have to wait for clock stablilization?
				}
				else{
					assert(0);
				}
			}
		}
		pthread_mutex_unlock(c->chmtx);
		
#if defined(USE_IIO)
		//TODO: we need command "tune adc idelay" instead of using libiio
		ret = iio_channel_attr_write_longlong(iio_device_get_channel(c->dev, 0), "sampling_frequency", fq);
		assert(0 == ret);
#else
		assert(0);		
#endif
		c->current_adc_frq = fq;
	}

	// TGC Config
#ifdef IDEAL_HUB
		// ADC ticks per one TGC tick. Must be even.
		// This is constant (10) defined  HUB2 Specification.
		tgc_set_ratio(channel, 10);

		adc_set_delay(channel, c->mPRAM->mPSET[si].Static[1]);		// c->mPRAM->mPSET[0].Static[1]
		adc_set_length(channel, c->mPRAM->mPSET[si].Static[3] - 1);		// c->mPRAM->mPSET[0].Static[3]

		tgc_set_active(channel, c->mPRAM->mPSET[si].Static[4]?1:0);		// c->mPRAM->mPSET[0].Static[4]
		//tgc_set_active(channel, 1);
		tgc_set_delay(channel, c->mPRAM->mPSET[si].Static[0]);		// c->mPRAM->mPSET[0].Static[0]
		tgc_set_length(channel,  c->mPRAM->mPSET[si].Static[7]);		// c->mPRAM->mPSET[0].Static[7]

		tgc_set_reg(channel, 0x0000, (short)(0x0000FFFF & c->mPRAM->mPSET[si].Static[9]));
		tgc_set_reg(channel, 0x0001, (short)(0x0000FFFF & c->mPRAM->mPSET[si].Static[10]));
		tgc_set_reg(channel, 0x0002, (short)(0x0000FFFF & c->mPRAM->mPSET[si].Static[11]));
		tgc_set_reg(channel, 0x0003, (short)(0x0000FFFF & c->mPRAM->mPSET[si].Static[12]));
		tgc_set_reg(channel, 0x0004, (short)(0x0000FFFF & c->mPRAM->mPSET[si].Static[13]));

		tgc_table_set(channel, (void*)(c->mPRAM->mPSET[si].Dynamic), c->mPRAM->mPSET[si].Static[7]);// c->mPRAM->mPSET[0].Static[7]

		tgc_run(c->number);//set bit PARAMETERS READY
#endif	

	c->current_pset++;
	if(c->current_pset == PRAM_SIZE){
		c->current_stbl++;
		c->current_pset = 0;
		if (c->current_stbl == STRAM_SIZE){
			c->current_stbl = 0;
		}
	}


}
void close_shot(HUB2_channel_t *c)
{
#if defined(USE_IIO)
	if(NULL != c->buf)
		iio_buffer_destroy(c->buf);
	c->buf = NULL;
	c->current_samples = 0;
#else
	assert(0);	
#endif
}
void report_shot(HUB2_channel_t *c)
{
	if(c->shot_count%5 == 0)
		printf("  SHOT #%9d\r", c->shot_count);
}

HUB2_channel_t* hub2_channel_create(int channel_number)
{
	int i, j;

	if ((0 > channel_number) || (2 < channel_number))
		return NULL;

	ch[channel_number].rxbuf = (unsigned short *)malloc(RX_BUFF_SIZE * sizeof(unsigned short));
	assert(NULL != ch[channel_number].rxbuf);
	if (NULL == ch[channel_number].rxbuf)
		return NULL;

	ch[channel_number].txbuf = (unsigned short *)malloc(TX_BUFF_SIZE * sizeof(unsigned short));
	assert(NULL != ch[channel_number].txbuf);
	if (NULL == ch[channel_number].txbuf){
		free(ch[channel_number].rxbuf);
		return NULL;
	}
	
	ch[channel_number].number = channel_number;
	ch[channel_number].state = CHANNEL_STATE_ABSENT;
	ch[channel_number].adc = -1;// file descriptor of adc driver
#if defined(USE_IIO)
	ch[channel_number].dev = NULL;
	ch[channel_number].buf = NULL;
#endif
	ch[channel_number].socket = -1;// socket file descriptor
	ch[channel_number].is_shot = -1;// ready to send shot index
	ch[channel_number].shot.pHeader = NULL;
	ch[channel_number].shot.pData = NULL;

	ch[channel_number].emulate_adc = 1;
	ch[channel_number].shot_mode = MEASURE_MODE_CONTINUES;	// 0 - single, 1 - continuous
	ch[channel_number].shot_send_mode = 0;		// 0 - simple, 1 - by request
	ch[channel_number].ovf_mode = 0;			// overflow mode (0,1,2)
	ch[channel_number].ctp_mode = 1;			// 0- extern, 1- emulator
	ch[channel_number].data_end = 0;			// 0 - little, 1 - big
	ch[channel_number].mPRAM = &mPRAM[channel_number];
	ch[channel_number].mSRAM = &mSRAM[channel_number];

	ch[channel_number].current_stbl = 0;		// crrent shot table
	ch[channel_number].current_pset = 0;		// current record in the current shot table
	
	ch[channel_number].current_adc_frq = 0;		
	ch[channel_number].current_samples = 0;

	ch[channel_number].rxbuf_size = RX_BUFF_SIZE * sizeof(unsigned short);
	ch[channel_number].rxbuf_size = RX_BUFF_SIZE * sizeof(unsigned short);

	for (i = 0; i < PRAM_SIZE; i++){
		mPRAM[channel_number].mPSET[i].Static[ 0] = 20;			// TX_DELAY
		mPRAM[channel_number].mPSET[i].Static[ 1] = 40;			// RX_DELAY
		mPRAM[channel_number].mPSET[i].Static[ 2] = 250000000;	// ADC FREQ.
		mPRAM[channel_number].mPSET[i].Static[ 3] = 1024;		// SAMPLES
		mPRAM[channel_number].mPSET[i].Static[ 4] = 0;			// TX_ACTIVE
		mPRAM[channel_number].mPSET[i].Static[ 5] = 1;			// RX_ACTIVE
		mPRAM[channel_number].mPSET[i].Static[ 6] = (channel_number+1) * 100000000 + i;			// DATA ID
		mPRAM[channel_number].mPSET[i].Static[ 7] = 8;			// TGC TABLE LENGTH
		mPRAM[channel_number].mPSET[i].Static[ 8] = 0x800;		// TGC VGA INITIAL GAIN
		mPRAM[channel_number].mPSET[i].Static[ 9] = 0x80;		// TGC REG 0
		mPRAM[channel_number].mPSET[i].Static[10] = 0x800;		// TGC REG 1 VGA GAIN
		mPRAM[channel_number].mPSET[i].Static[11] = 0x03;		// TGC REG 2
		mPRAM[channel_number].mPSET[i].Static[12] = 0x48;		// TGC REG 3
		mPRAM[channel_number].mPSET[i].Static[13] = 25;			// TGC REG 4
		for (j = 0; j < PSET_DYNAMIC_SIZE; j++){
														/*  Value       Delta      Count  */
			mPRAM[channel_number].mPSET[i].Dynamic[j] = (0x800 << 20) | (0 << 14) | (4 << 0);
		}
	}

	for (i = 0; i < STRAM_SIZE; i++)
		for (j = 0; j < PRAM_SIZE; j++)
			mSRAM[channel_number].mSTBL[i].mPSETindex[j] = j;

	return (HUB2_channel_t*)&ch[channel_number];
}
void hub2_channel_destroy(HUB2_channel_t *c)
{
	if (NULL != ch)
		return;

	if (NULL != c->rxbuf)
		free(c->rxbuf);

	if (NULL != c->txbuf)
		free(c->txbuf);

	c->rxbuf_size = -1;
	c->number = -1;
	c->state = -1;
	c->adc = -1;
	c->socket = -1;
	c->is_shot = -1;

	return;
}
int hub2_channel_open(HUB2_channel_t *c, void* ptr)
{
	int ret;
	struct iio_context *ctx = (struct iio_context*)ptr;
	unsigned int dev_count, dev_i;
	struct iio_device* pdev;
	const char* dev_name = NULL;

#if defined(USE_IIO)
	dev_count = iio_context_get_devices_count(ctx);
	for(dev_i=0;dev_i<dev_count;dev_i++){
		pdev = iio_context_get_device(ctx,dev_i);
		dev_name = iio_device_get_name(pdev);
		if( 0 == strcmp(c->number == 0 ? "axi_ad9467_N0" : "axi_ad9467_N1",dev_name) ){
			printf("iio device #%d: %s\n", dev_i, dev_name);
			c->dev = pdev;
			break;
		}else{
			c->dev = NULL;
		}
	}
#else
	assert(0);
#endif

	if(c->dev != NULL){
#ifdef IDEAL_HUB
		tgc_ctrl_open(c->number);
		tgc_table_open(c->number);
#else
	assert(0);
#endif

#if defined(USE_IIO)
		iio_channel_enable(iio_device_get_channel(c->dev, 0));
		//hack - to calibrate fpga input delay:
		ret = iio_channel_attr_write_longlong(iio_device_get_channel(c->dev, 0), "sampling_frequency", DEFAULT_MCLK_FREQ_HZ);
		assert(0==ret);
		iio_channel_attr_write(iio_device_get_channel(c->dev, 0), "test_mode", "off");
#else
		assert(0);
#endif

		//TODO: c->adc = 1; ?
	}else{
		c->state = CHANNEL_STATE_ABSENT;
		return -1;
	}
	c->state = CHANNEL_STATE_CONNECT;
	return OK;
}
int hub2_channel_close(HUB2_channel_t *c)
{
	// TODO: check channel state and make appropriate actions
#if defined(USE_IIO)
#else
	assert(0);
#endif

#ifdef IDEAL_HUB
	tgc_ctrl_close(c->number);
	tgc_table_close(c->number);
#endif    
	return OK;
}

int hub2_channel_netcfg(HUB2_channel_t* c, listener_state_t *announce)
{
    unsigned short base_port;
    int x;
    char *p = NULL;
    
	assert(c);
	if (NULL == announce  && NULL == c->listener){
		c->serv_addr.sin_family = AF_INET;
		c->serv_addr.sin_port = htons(30000);
#ifdef WIN32
		c->serv_addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");//INADDR_ANY; inet_addr("127.0.0.1"); inet_addr("192.168.100.15")
#else
		c->serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");//INADDR_ANY; inet_addr("127.0.0.1"); inet_addr("192.168.100.15")
#endif
		return OK;
	}
    // TODO: remove this cherezjopie
    if(NULL != announce && NULL == c->listener)
        c->listener = announce;
        
	assert(c->listener->msg_addr != NULL);

	if (!c->listener->msg_ok) return -1;
    
	c->serv_addr.sin_family = AF_INET;
    
    // get port from announce
    p = strstr(c->listener->msg, "BASEPORT:");
    if(NULL == p)
    {
        printf("ERROR: unable to find baseport number in the announce\n");
        base_port = DEFAULT_HOST_TCP_PORT;
    }else{
        x = atoi(p+sizeof("BASEPORT:")-1);
        if(x>65535){
            printf("ERROR: wrong baseport %d in the announce\n",x);
            x = DEFAULT_HOST_TCP_PORT;
        }
        base_port = (unsigned short)x;
    }
    
	c->serv_addr.sin_port = htons(base_port); 
#ifdef WIN32
	c->serv_addr.sin_addr.S_un.S_addr = inet_addr(c->listener->msg_addr);//INADDR_ANY; inet_addr("127.0.0.1"); inet_addr("192.168.100.15")
#else
    c->serv_addr.sin_addr.s_addr = inet_addr(c->listener->msg_addr);//INADDR_ANY; inet_addr("127.0.0.1"); inet_addr("192.168.100.15")
#endif  
 
	return OK;
}
int hub2_channel_connect(HUB2_channel_t *c)
{
	int rv = -1;
#ifdef WIN32
	c->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (c->socket == INVALID_SOCKET)
	{
		perror("Invalid socket");
		return c->socket;
	}

	rv = connect(c->socket, (SOCKADDR*)&c->serv_addr, sizeof(sockaddr_in));
	if (rv)
	{
		printf("Connect failed %u\r", WSAGetLastError());
		closesocket(c->socket);
		return rv;
	}
#else 
	c->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (c->socket < 0){
		perror("Error opening socket");
		return rv;
	}

    rv = connect(c->socket,(struct sockaddr *)&(c->serv_addr),sizeof(c->serv_addr));
	if (rv < 0) {
		close(c->socket);
		perror("Error on connecting");
		return rv;
	}
    return 0;   
#endif    
	c->state = CHANNEL_STATE_IDLE;
	return OK;
}

int hub2_channel_disconnect(HUB2_channel_t *c)
{
	if (-1 < c->socket){
#ifdef WIN32
#define SHUT_RDWR SD_BOTH
#endif
		shutdown(c->socket, SHUT_RDWR);
#ifdef WIN32
		closesocket(c->socket);
#else
		close(c->socket);
#endif
		c->socket = -1;
	}
	return OK;
}

void* hub2_channel_thread(void *p)
{
	HUB2_channel_t* c = (HUB2_channel_t*)p;
	int retries = 0;

	while (CHANNEL_STATE_EXIT != c->state){
		switch (c->state){
		case CHANNEL_STATE_CONNECT:
			retries++;
			printf(".. connecting %d ... ", retries);
			if (OK != hub2_channel_netcfg(c, c->listener)){
				printf("ERROR : hub2_channel_netcfg(). Exit...");
				c->state = CHANNEL_STATE_EXIT;
				break;
			}
			if (OK != hub2_channel_connect(c)){
				break;
			}
			printf("  connected!\n");
			c->state = CHANNEL_STATE_IDLE;
			retries = 0;
			break;
		case CHANNEL_STATE_IDLE:
			if (OK == receive_command(c)){
				process_command(c);
			}
			else{
				c->state = CHANNEL_STATE_CONNECT;
			}
			break;
		case CHANNEL_STATE_MEASURE:
			if (0 < have_command(c)){
				if (OK == receive_command(c)){
					process_command(c);
				}
				else{
					c->state = CHANNEL_STATE_CONNECT;
				}
				if (c->state != CHANNEL_STATE_MEASURE){
					close_shot(c);
					break;
				}
			}
			if (c->state == CHANNEL_STATE_MEASURE){
				if (0 < have_shot(c)){
					set_shot(c);
					if (OK == get_shot(c)){
						if (OK == send_shot(c)){
							release_shot(c);
							report_shot(c);
							if (c->shot_mode == MEASURE_MODE_SINGLE)
								c->state = CHANNEL_STATE_IDLE;
						}
						else{
							close_shot(c);
							c->state = CHANNEL_STATE_CONNECT;
						}
					}
				}
			}

			break;
		case CHANNEL_STATE_CONFIG:
			if (OK == receive_command(c)){
				process_command(c);
			}
			else{
				c->state = CHANNEL_STATE_CONNECT;
			}
			break;
		case CHANNEL_STATE_ABSENT:
			printf("Channel %d FAILURE...\n", c->number);
			c->state = CHANNEL_STATE_EXIT;
			break;
		case CHANNEL_STATE_ERROR:
			break;
		case CHANNEL_STATE_EXIT:
			printf("..exit..");
			break;
		default:
			process_exit(c);
			assert(0);
			return NULL;
		}
	}
	return NULL;
}
