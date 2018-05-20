/*
    2016.08.16
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <pthread.h>
#include <assert.h>
#include <iio.h>
#include <time.h> /* for benchmarking */

#include "HUB2.h"
#include "HUB2_channel.h"
#include "HUB2_commander.h"
#include "HUB2_board_info.h"
#include "pins.h"
#include "clk_idt5p49v6914a.h"

void iio_test(int mode);

/*
    29.08.2016
    Debug version of the system.
    Simplified HUB2 work algorithm:
    1. Configuration. Принимает он сервера ВСЕ конфигурационные данные. Это:
        - полная таблица mPARAM ( 8192 x mPSET )
        - полная таблица mSTRAM ( 1024 x mSTBL )
        - регистры HUB (в том числе регистр rSTBL который указывает номер текущей mSTBL)
        - регистры аналоговой части 
                
    2. Waiting for all READY.
        Устройство ставит READY когда
            - оно сконфигурено
            - есть место для одного измерения. (128 * 1024)сэмплов * 16(бит на сэмпл) + Header_Size. Хидера лежат в отдельном месте.  
    3. Waiting for trigger. Из триггера получаем:
        - координату
        - (опционально) номер mSTBL (номер short table) которым надо пользоватся в данном измерении.
    4. Выставляем DO (мастер) / ждем DO (слэйв).
        Сигнал DO сбрасывает READY всем устройствам на бэкплэйне
*/

static void wait_key_pressed(void){
//#ifdef DEBUG
	getchar();
//#endif
}
static int is_announce(char *msg, int length)
{
    int rv;
    rv = strncmp(msg,"i-Deal Unius Server", sizeof("i-Deal Unius Server")-1 );
    return rv==0 ? 1:0;    
}

// get sockaddr, IPv4 or IPv6:
static void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/* Reference source: http://www.sql.ru/forum/691693/udp-soket-pod-linux-priyom-soobshheniy */
static void* udp_listener_thread(void* p) {
    listener_state_t *pState = (listener_state_t*)p;
    int m_sock;                     // socket file descriptor
    struct sockaddr_in m_addr;      // Local interface address
    struct sockaddr_storage their_addr;
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];
    char recvmsg[1024];
    int bytesrecv;

    if ((m_sock = socket(PF_INET, SOCK_DGRAM, 0/* or IPPROTO_UDP*/)) < 0) {
        perror("socket");
        return NULL;
    }

    bzero(&m_addr, sizeof(m_addr));
	m_addr.sin_addr.s_addr = htonl(INADDR_ANY);//htonl(0xC0A80013);
    //inet_aton(DBG_HOST_IP, &(m_addr.sin_addr));
    m_addr.sin_family   = AF_INET;  // must be AF_INET!
    m_addr.sin_port     = htons(pState->port); // 0 - auto port assign

    if (bind(m_sock, (struct sockaddr*)&m_addr, sizeof(m_addr)) < 0) {
        perror("bind");
        close(m_sock);
        return NULL;
    }

    addr_len = sizeof(their_addr);
    printf("Listener: listen\n");
    while(CMD_LISTEN == (listener_cmd_t)pState->command) {
        if ((bytesrecv = recvfrom(m_sock, recvmsg, 1024, MSG_NOSIGNAL, (struct sockaddr*)&their_addr, &addr_len)) < 0) {
            perror("recvfrom");
            close(m_sock);
            return NULL;
        }
        printf("Message received! Size: %d\n", bytesrecv);
        printf("listener: got packet from %s\n",
                            inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof(s) ) );
        printf("Message:\n");
        printf("-------------------\n");
        printf("%s",recvmsg);
        printf("-------------------\n");
        
        if( is_announce(recvmsg,bytesrecv) ){
            //TODO: protect with critical section
            memcpy(pState->msg,recvmsg,bytesrecv);
            memcpy(pState->msg_addr,s,sizeof(s));
            pState->msg_size = bytesrecv;
            pState->msg_ok = 1;
            pState->command = CMD_EXIT;
        }
    }

    close(m_sock);
    return NULL;
}

static listener_state_t listener_state;
static struct iio_context *ctx;
static board_info_t board_info;

static mPRAM_t mPARAM;
static mSRAM_t mSRAM;
static pthread_mutex_t channels_mutex = PTHREAD_MUTEX_INITIALIZER;

int main(void)
{
    int rv,i,j,slot_id, backplane, master;
	pthread_t listener_id;
	pthread_t channel_id[2];
	HUB2_channel_t* c[2] = { NULL, NULL };

	printf(" *** iDeal_hub application (2018.02.19 11:00) ***\n\n");

	// 0. prepare data (for debug only)
	memset( (void*)&mSRAM, 0, sizeof(mSRAM_t));
	for(i=0;i<PRAM_SIZE;i++){
		mPARAM.mPSET[i].Static[ 0] = 20;			// TX_DELAY
		mPARAM.mPSET[i].Static[ 1] = 40;			// RX_DELAY
		mPARAM.mPSET[i].Static[ 2] = 250000000;		// ADC FREQ.
		mPARAM.mPSET[i].Static[ 3] = 1024;			// SAMPLES
		mPARAM.mPSET[i].Static[ 4] = 0;				// TX_ACTIVE
		mPARAM.mPSET[i].Static[ 5] = 1;				// RX_ACTIVE
		mPARAM.mPSET[i].Static[ 6] = 666;			// DATA ID
		mPARAM.mPSET[i].Static[ 7] = 8;				// TGC TABLE LENGTH
		mPARAM.mPSET[i].Static[ 8] = 0x800;			// TGC VGA INITIAL GAIN (12 bit length, 4095 max value)
		mPARAM.mPSET[i].Static[ 9] = 0x80;			// TGC REG 0
		mPARAM.mPSET[i].Static[10] = 0x800;			// TGC REG 1 (VGA GAIN)
		mPARAM.mPSET[i].Static[11] = 0x03;			// TGC REG 2
		mPARAM.mPSET[i].Static[12] = 0x48;			// TGC REG 3
		mPARAM.mPSET[i].Static[13] = 25;			// TGC REG 4
		for(j=0;j<PSET_DYNAMIC_SIZE;j++){
										/*  Value    Delta   Count   */
										/*  12 bits  6 bits  14 bits */
			mPARAM.mPSET[i].Dynamic[j] = (0x800<<20)|(0<<14)|(4<<0);
		}
	}

	// 1. Who am I on backplane?
	//  1.1. Am I on the backplane?
	//  1.2. Am I master on the backplane?
	//  1.3. What is my slot number on backplane.

#ifdef IDEAL_HUB // i.e. iDeal_hub.bit

	if(-1 != hub2_mngr_open() ){

		printf(" HUB2 Commander IP Core version %08X\n",hub2_mngr_get_version() );

		backplane 	= hub2_mngr_is_backplane();
		master 		= hub2_mngr_is_master();
		slot_id 	= hub2_mngr_get_slot_id();

		printf(" Backplane: %s\n", 0==backplane?"NO":"YES");
		printf(" Master   : %s\n", 0==master?"NO":"YES");
		printf(" Slot ID  : %d\n", slot_id);

	}else{
		printf("Unable to open hub2 commander. Press any key to Exit...\n");
		wait_key_pressed();
		return EXIT_FAILURE;
	}

	rv = pins_open();
	if(rv != OK){
	   printf("Unable to get GPIOs. Press any key to Exit...\n");
	   wait_key_pressed();
	   return EXIT_FAILURE;
	}

	if( backplane ){
		if( master ){
			pin_mclk_set(ON);
			pin_led_set(MASTER,ON);
		}else{
			pin_mclk_set(OFF);
			pin_led_set(MASTER,OFF);
		}
	}else{
		master = 1;
		pin_led_set(MASTER,ON);
		pin_mclk_set(ON);
	}

#else

   rv = pins_open();
   if(rv != OK){
       printf("Unable to get GPIOs. Press any key to Exit...\n");
	   wait_key_pressed();
       return EXIT_FAILURE;
   }
   if(pins_is_backplane()){
	   if(pins_is_master()){
		   pin_mclk_set(1);
		   pin_led_set(2,1);
		   master = 1;
	   }else{
		   pin_mclk_set(0);
		   pin_led_set(2,0);
		   master = 0;
	   }
	   slot_id = pins_slot_id();
	   backplane = 1;
   }else{
	   pin_mclk_set(1);
	   slot_id = 0;
	   pin_led_set(2,1);
	   backplane = 0;
	   master = 1;
   }
   printf("Backplane: %c. Master %c. Slot %d.\n", backplane?'Y':'N', master?'Y':'N', slot_id);

#endif

   rv = clk_open();
   if( master ){
	   // Enable and set defaults to ADC clock (IDT 5P49V6914)
	   	rv = clk_set_mhz(DEFAULT_MCLK_FREQ_MHZ);
   }else{
	   // Disable and off ADC clock (IDT 5P49V6914) because board is not a master
	   // TODO: disable clock generator for slave bards
	rv = clk_shutdown();
   }
   rv = clk_close();

   board_info.backplane = backplane;
   board_info.master = master;
   board_info.slot_id = slot_id;
   board_info.channels = CHANNELS;
   board_info.clk_freq_hz = DEFAULT_MCLK_FREQ_MHZ * 1000000;

// 2. ADC Channel object(s) creation
   //iio_test(0); return 0;//just for test and debug
   ctx = iio_create_local_context();//iio_create_default_context();
   assert(ctx != NULL);
   rv = iio_context_set_timeout(ctx,100); // Timeout is in milliseconds. 10 is also works good.
   assert(rv==0);

// 3. open hardware driver(s) .
    for(i=0;i<CHANNELS;i++){
        c[i] = hub2_channel_create(i);
        if(NULL != c[i]){
        	c[i]->chmtx = (pthread_mutex_t*)&channels_mutex;
        	c[i]->current_adc_frq = DEFAULT_MCLK_FREQ_MHZ * 1000000;
			c[i]->board_info = &board_info;

            if(0 == hub2_channel_open( c[i], (void*)ctx ) ){
#ifdef IDEAL_HUB
            	hub2_mngr_set_ch_en(i, 1);
#endif
                printf("ADC channel %d opened\n", i);
            }else{
#ifdef IDEAL_HUB
            	hub2_mngr_set_ch_en(i, 0);
#endif
                printf("ADC channel %d open FAILED\n", i);
            }
        }else{
#ifdef IDEAL_HUB
        	hub2_mngr_set_ch_en(i, 0);
#endif
            printf("ADC channel %d create FAILED\n", i);
        }
    }
    for(i=0,rv=0;i<CHANNELS;i++){
        if( NULL!=c[i] ) rv = 1;
    }
    if(0==rv){
        printf("Critical malfunction. Exit...\n");
		wait_key_pressed();
        return EXIT_FAILURE;
    }

#ifdef IDEAL_HUB
    // DEBUG ONLY:
    hub2_mngr_set_swdo(1);
    hub2_mngr_set_do(1);
    hub2_mngr_fpga_reset();
#endif

// 4. Read settings from EEPROM and do setup

// 5. Getting IP address
    // 5.1. Is there setting "use static ip"?
    // 5.2. проверяем отличаеся или текущий айпи от дефолтного, если отличается то ок, если нет ждем когда по телнету нам дадут новый

// 5A. Getting UDP broadcast announce from host.
 listener_state.port = DBG_HOST_UDP_PORT;
 listener_state.command = CMD_LISTEN;
 rv = pthread_create(&listener_id, NULL, udp_listener_thread, (void*)&listener_state);
 if (rv != OK) {
   perror("Creating the first thread");
   wait_key_pressed();
   return EXIT_FAILURE;
 }
 //for debug:
 //sleep(10);// let listener thread to catch announce
 //listener_state.command = CMD_EXIT;//
 // 4B. wait for listener thread death
 rv = pthread_join(listener_id, NULL);
 if (rv != OK) {
   perror("Joining the second thread");
   wait_key_pressed();
   return EXIT_FAILURE;
 }else{
	 printf("\nTo connect to server pressany key\n\n");
	 //printf("\nListener thread started...\n\n");
 }
 // At this point we have host ip address.
 // It is in the istener_state.msg_addr.

 wait_key_pressed();

 for (i = 0; i < CHANNELS; i++){
	 if (NULL != c[i]){
		 if (OK != hub2_channel_netcfg(c[i], &listener_state)){
			 printf("ERROR : hub2_channel_netcfg(). Exit...");
			 wait_key_pressed();
			 return EXIT_FAILURE;
		 }
	 }
 }
 
// 6. Run broadcast process "Telemety"
//  Sends broadcast udp packets with current state report
    /*
    "I am HUB2 v.XXXXXX" 
    On Backplane
    Master (or Slave)
    Slot number XX
    Packet interval is 500 miliseconds
    State - ХХХХХХ
    ...
    
    Telemetry packet size - no more than 508 bytes. This will help
    to avoid packet fragmentation during transmission.
    The 508 because max. MTU size which supported by ALL network
    hardware is 576 bytes
    */

 for (i = 0; i < CHANNELS; i++){
	 if (NULL != c[i]){
		 // 6. Connecting is inside nub2_channel_thread()
		 //if (OK != hub2_channel_connect(c[i], "192.168.100.15")){
		 //	 hub2_channel_disconnect(c[i]);
		 //	 printf("ERROR : unable to connect. Exit...");
		 //	 wait_key_pressed();
		 //	 return EXIT_FAILURE;
		 //}
		 // 7. run channel threads
		 rv = pthread_create(&channel_id[i], NULL, hub2_channel_thread, (void*)c[i]);
		 if (rv != OK) {
			 perror("Creating the first thread");
			 wait_key_pressed();
			 return EXIT_FAILURE;
		 }
	 }
 }
// N1. Знакомимся с соседями по бэкплэйну (если нужно. это делается только по команде сервера)
//  N.1. Сервер должен ВСЕМ устройствам включить режим "Идентификация соседей"
//  N.2. Выбираем мастер и шлем ему команду "Покажи соседей"
//  N.3. Собираем телеметрию.
//  N.4. Находим соседей мастера.
//  N.5. Command  to master "do not show neighbors"
//  N.6. If master more than one then repeat 5.2.

 while(1){ ; }
// disconnect, close and exit...
 for (i = 0; i < CHANNELS; i++){
	 if (NULL != c[i]){
		 hub2_channel_disconnect(c[i]);
		 hub2_channel_destroy(c[i]);
		 rv = pthread_join(channel_id[i], NULL);
		 if (rv != OK) {
			 perror("Joining the channel thread");
			 wait_key_pressed();
			 return EXIT_FAILURE;
		 }
	 }
 }
 printf("HUB2 Compleated. Press any key to exit...");
 wait_key_pressed();
 pins_close();
 hub2_mngr_close();
 iio_context_destroy(ctx);
 return 0;
}

/*
 * Test IIO Library
 * mode = 0 ==> test mode is off
 *
 */
// 2. ADC Channel object(s) creation
void iio_test(int mode){
   static struct iio_context *ctx;
   static struct iio_buffer *buffer[2];
   struct iio_device *dev[2];
   int ret,counter=0;
   struct timespec tA,tB;
   short* buf_data;
   unsigned long volume=0;

   ctx = iio_create_default_context();
   assert(ctx != NULL);

   dev[0] = /*iio_context_get_device(ctx, 0);*/iio_context_find_device(ctx, "iio:device1");
   assert(dev[0] != NULL);
   dev[1] = /*iio_context_get_device(ctx, 2);*/iio_context_find_device(ctx, "iio:device2");
   assert(dev[1] != NULL);

   //just for sureness
   printf("xIIO dev-1 name: %s\n", iio_device_get_name( dev[0]) );
   printf("xIIO dev-2 name: %s\n", iio_device_get_name( dev[1]) );

   printf("IIO dev-1: channels %d\n",iio_device_get_channels_count( dev[0]) );
   printf("IIO dev-2: channels %d\n",iio_device_get_channels_count( dev[1]) );

   iio_channel_enable(iio_device_get_channel(dev[0], 0));
   iio_channel_enable(iio_device_get_channel(dev[1], 0));

   //hack - to calibrate fpga input delay
   ret = iio_channel_attr_write_longlong(iio_device_get_channel(dev[0], 0), "sampling_frequency", DEFAULT_MCLK_FREQ_HZ);
   assert(0==ret);
   ret = iio_channel_attr_write_longlong(iio_device_get_channel(dev[1], 0), "sampling_frequency", DEFAULT_MCLK_FREQ_HZ);
   assert(0==ret);

   //off midscale_short pos_fullscale neg_fullscale checkerboard pn_long pn_short one_zero_toggle
   iio_channel_attr_write(iio_device_get_channel(dev[0], 0), "test_mode", mode==0?"off":"checkerboard");
   iio_channel_attr_write(iio_device_get_channel(dev[1], 0), "test_mode", mode==0?"off":"checkerboard");


   ret = iio_device_set_kernel_buffers_count(dev[0], 16);
   printf("IIO dev-1: set kernell buffers number to 16 return %d\n",ret);
   ret = iio_device_set_kernel_buffers_count(dev[1], 16);
   printf("IIO dev-2: set kernell buffers number to 16 return %d\n",ret);

   buffer[0] = iio_device_create_buffer(dev[0], 1024*64, false);
   assert(buffer[0] != NULL);
   buffer[1] = iio_device_create_buffer(dev[1], 1024*64, false);
   assert(buffer[1] != NULL);

   clock_gettime(CLOCK_REALTIME, &tA);
#define DMA_LOOPS (10000)
   while(counter++<DMA_LOOPS){
	   ret = iio_buffer_refill(buffer[0]);
	   assert(-1<ret);
	   volume += ret;
	   ret = iio_buffer_refill(buffer[1]);
	   assert(-1<ret);
	   volume += ret;
   }
   clock_gettime(CLOCK_REALTIME, &tB);
   buf_data = (short*)iio_buffer_start(buffer[0]);

   printf("Received by dma: %lu bytes. %s.\n",volume, volume==(unsigned long)DMA_LOOPS*1024*64*2*2?"OK":"FAILED");
   printf("RATE = %f GB/s\n", 1024*64*2*2.0*DMA_LOOPS / (1000000000.0*(tB.tv_sec - tA.tv_sec)+(tB.tv_nsec - tA.tv_nsec)) );

   for(counter=0;counter<500;counter++){
	   if(0==counter%10) printf("\n");
	   if(0==mode)
		   printf("%6d ", (short)buf_data[counter]);
	   else
		   printf("%04x ", (unsigned short)buf_data[counter]);
   }
   printf("\n");

   iio_buffer_destroy(buffer[0]);
   iio_buffer_destroy(buffer[1]);
   iio_context_destroy(ctx);
}

