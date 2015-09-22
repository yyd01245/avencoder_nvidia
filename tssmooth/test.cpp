
//gcc ts_send.c -o ts_send -lpthread

#include "tssmooth.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <pthread.h>

int main()
{
	int  listen_udp_port = 6666;
	char *dst_udp_ip = "192.168.100.106";
	int  dst_udp_port = 50730;

	int bit_rate = 5*1024*1024;
	int buffer_max_size = 100*1024*1024;

	InputParams_tssmooth pInputParams;

	pInputParams.index = 1;
	pInputParams.listen_udp_port = 0;
	strncpy(pInputParams.dst_udp_ip,dst_udp_ip,sizeof(pInputParams.dst_udp_ip));
	pInputParams.dst_udp_port = dst_udp_port;
	pInputParams.bit_rate = bit_rate;
	pInputParams.buffer_max_size = buffer_max_size;

	ts_smooth_t *p = init_tssmooth(&pInputParams);;

	/*///////////test/////////////////
	struct timeval tv1,tv2;
	gettimeofday(&tv1, NULL);
	memcpy(send_buffer,recv_buffer,5*1024*1024);
	gettimeofday(&tv2, NULL);

	printf("time = %d %d\n",tv1.tv_sec,tv1.tv_usec);
	printf("time = %d %d\n",tv2.tv_sec,tv2.tv_usec);

	printf("time = %d\n",(tv2.tv_sec - tv1.tv_sec)*1000 + (tv2.tv_usec - tv1.tv_usec)/1000);
	
	*////////////test/////////////////

	//while(1)
		sleep(1);
	
	return 0;
	
}
