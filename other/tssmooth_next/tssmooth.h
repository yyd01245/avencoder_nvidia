#ifndef __TS_SMOOTH_H__
#define __TS_SMOOTH_H__

typedef void ts_smooth_t;

typedef struct
{
	int index;
	char listen_udp_ip[32];
	int  listen_udp_port;
	char dst_udp_ip[32];
	int  dst_udp_port;
	int bit_rate;//bps
	int buffer_max_size;

}InputParams_tssmooth;

ts_smooth_t* init_tssmooth(InputParams_tssmooth *pInputParams);
unsigned long get_real_send_bytes(ts_smooth_t *smooth);
int uninit_tssmooth(ts_smooth_t *smooth);

#endif


