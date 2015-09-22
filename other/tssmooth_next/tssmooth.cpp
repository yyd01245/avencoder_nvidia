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
#include <errno.h>

#include <semaphore.h>
#include <pthread.h>

#define  BUFFER_POOL_NUMBER 2

typedef struct
{
	int buffer_size;
	pthread_mutex_t m_mutex;
	int buffer_bytes;
	char *data;
}buffer_t;


typedef struct
{
	char listen_udp_ip[32];
	int  listen_udp_port;
	char dst_udp_ip[32];
	int  dst_udp_port;

	int bit_rate;//b
	int buffer_max_size;

	//use for inside

	buffer_t buffer_pool[BUFFER_POOL_NUMBER];

	pthread_t read_thread_id;
	pthread_t write_thread_id;

	int read_thread_status; // 1 wait read data ; 0 not wait read data 
	int write_thread_status;// 1 wait sem data ; 0 not wait sem data

	sem_t m_sem;

	//for statistics
	unsigned long real_send_bytes;
}tssmooth_instanse;


int applyforUDPPort(char *ip,int *port)
{
	struct sockaddr_in bindaddr;
	socklen_t len;
	
	struct sockaddr_in servaddr;
	memset(&servaddr,0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr(ip);
	servaddr.sin_port = htons(*port);
	
	int test_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if(test_socket == -1)
		return -1;
	
	int optval = 1;
	if ((setsockopt(test_socket,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(int))) == -1)
	{
		close(test_socket);
		return -2;
	}

	if(bind(test_socket, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
	{
		close(test_socket);
		return -3;
	}
	
	if( 0 == getsockname( test_socket, ( struct sockaddr* )&bindaddr, &len ))
	{
		//printf("%s:%d\n",inet_ntoa(bindaddr.sin_addr),ntohs(bindaddr.sin_port));
		
		*port = ntohs(bindaddr.sin_port);
	}
	else
	{
		close(test_socket);
		return -4;
	}
	
	close(test_socket);
	return 0;
}

void *ts_recv_thread(void *arg)
{
	tssmooth_instanse *param = NULL;
	param = (tssmooth_instanse *)arg;


	struct sockaddr_in servaddr;
	memset(&servaddr,0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(param->listen_udp_port);
	
	int socket_id = socket(AF_INET, SOCK_DGRAM, 0);
	if(socket_id == -1)
		return NULL;
	
	int optval = 1;
	if ((setsockopt(socket_id,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(int))) == -1)
	{
		close(socket_id);
		return NULL;
	}
	
	int send_recv_size = 1024*1024;	
	int ret = setsockopt(socket_id, SOL_SOCKET, SO_RCVBUF, (const char *)&send_recv_size, sizeof(int));
	if(ret==-1)
		printf("======set tss recv buff error\n");

	if(bind(socket_id, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
	{
		close(socket_id);
		return NULL;
	}

	struct sockaddr_in cliaddr;
	int addr_len = sizeof(struct sockaddr_in);
	char recv_buffer[2048];
	int recv_len = -1;

	int ret_int = -1;
	int write_index = 0;
	buffer_t *write_buffer = NULL;

	int flag_ok = 0;

	while (1)
	{
		memset(recv_buffer, 0, sizeof(recv_buffer));
		param->read_thread_status = 1;
		recv_len = recvfrom(socket_id, recv_buffer, sizeof(recv_buffer), 0, (struct sockaddr *)&cliaddr, (socklen_t*)&addr_len);
		param->read_thread_status = 0;
		if(recv_len <= 0)
			continue;

		flag_ok = 0;
		for(int i = 0;i < BUFFER_POOL_NUMBER;i++)
		{
			if(write_index == BUFFER_POOL_NUMBER)
				write_index = 0;

			write_buffer = &param->buffer_pool[write_index];

			ret_int = pthread_mutex_trylock(&write_buffer->m_mutex);
			if(ret_int == 0)
			{
				//success
				flag_ok = 1;
				break;
			}
			else if(ret_int == EBUSY)
			{
				//already locked
			}
			else
			{
				//error
				printf("ERROR : pthread_mutex_trylock error ret=%d.\n",ret_int);
			}

			write_index++;
		}

		if(flag_ok == 0)
		{
			printf("ERROR : no buffer used!!!!!!\n");
			continue;
		}
		
		if(write_buffer->buffer_bytes + recv_len > write_buffer->buffer_size)
		{
			//reset
			write_buffer->buffer_bytes = 0;
			pthread_mutex_unlock(&write_buffer->m_mutex);
			printf("Waring,buffer is too small,or send is slow,it will reset!\n");
			continue;
		}

		memcpy(write_buffer->data + write_buffer->buffer_bytes,recv_buffer,recv_len);
		write_buffer->buffer_bytes += recv_len;	

		if(param->write_thread_status == 1)
			sem_post(&param->m_sem);
		
		pthread_mutex_unlock(&write_buffer->m_mutex);
	}
}


void *ts_send_thread(void *arg)
{
	tssmooth_instanse *param = NULL;
	param = (tssmooth_instanse *)arg;
	
	struct sockaddr_in servaddr;
	memset(&servaddr, 0,sizeof(servaddr));
	servaddr.sin_family = AF_INET;	
	servaddr.sin_addr.s_addr = inet_addr(param->dst_udp_ip);	
	servaddr.sin_port = htons(param->dst_udp_port);

	int socket_id = socket(AF_INET, SOCK_DGRAM, 0);
	if(socket_id == -1)
		return NULL;
	
	int send_buf_size = 0;	
	setsockopt(socket_id, SOL_SOCKET, SO_SNDBUF, (const char *)&send_buf_size, sizeof(int));

	int ret_int = -1;
	int read_index = 0;
	buffer_t *read_buffer = NULL;

	int send_index = 0;
	int real_send_len = -1;
	int want_send_len = 7*188;
	int have_send_len = want_send_len;
	

	struct timeval tv;
	long long us_time1,us_time2;

	long have_delay = -1;
	
	while(1)
	{
		param->write_thread_status = 1;
		sem_wait(&param->m_sem);
		param->write_thread_status = 0;

		if(read_index == BUFFER_POOL_NUMBER)
			read_index = 0;
		
		read_buffer = &param->buffer_pool[read_index];

		pthread_mutex_lock(&read_buffer->m_mutex);

		send_index = 0;
		real_send_len = -1;
		have_send_len = 0;
	
		while(1)
		{

			if(read_buffer->buffer_bytes - send_index > want_send_len)
			{
				have_send_len = want_send_len;
			}
			else if(read_buffer->buffer_bytes - send_index > 0)
			{
				have_send_len = read_buffer->buffer_bytes - send_index;
			}
			else //read_buffer->buffer_bytes - send_index <= 0
			{
				//error
				break;
			}

			have_delay = ((double)have_send_len/param->bit_rate)*8*1000*1000;

			gettimeofday(&tv, NULL);
			us_time1 = tv.tv_sec*1000*1000 + tv.tv_usec;
			real_send_len = sendto(socket_id, read_buffer->data+send_index, have_send_len, 0, (struct sockaddr *)&servaddr, sizeof(servaddr));			
			if(real_send_len > 0)
			{
				send_index+=real_send_len;
				param->real_send_bytes += real_send_len;
			}
			else
			{
				send_index+=have_send_len;
			}
			
			gettimeofday(&tv, NULL);
			us_time2 = tv.tv_sec*1000*1000 + tv.tv_usec;

			if(have_delay-(us_time2-us_time1) > 0)
				usleep(have_delay-(us_time2-us_time1));

			//printf("=======have_delay=%ld us_time2-us_time1=%lld have_send_len =%d errno=%d real_send_len=%d \n",have_delay,us_time2-us_time1,have_send_len,errno,real_send_len);

			

		}

		read_buffer->buffer_bytes = 0;

		pthread_mutex_unlock(&read_buffer->m_mutex);
		
		read_index++;
	}
}


ts_smooth_t* init_tssmooth(InputParams_tssmooth *pInputParams)
{
	if(pInputParams == NULL)
	{        
		fprintf(stderr ,"libtssmooth: Error paraments..\n");        
		return NULL;    
	}

	
	tssmooth_instanse *p_instanse = (tssmooth_instanse *)malloc(sizeof(tssmooth_instanse));
	if(p_instanse == NULL)
	{
		fprintf(stderr, "libtssmooth malloc tssmooth_instanse failed...\n");
		return NULL;
	}

	if(pInputParams->listen_udp_port <= 0)
	{
		strncpy(pInputParams->listen_udp_ip,"127.0.0.1",sizeof(pInputParams->listen_udp_ip));
		pInputParams->listen_udp_port = 0;
		int ret = applyforUDPPort(pInputParams->listen_udp_ip,&pInputParams->listen_udp_port);
		if(ret < 0)
		{
			fprintf(stderr, "+++libtssmooth applyforUDPPort is error %d ,ret = %d\n",pInputParams->listen_udp_port,ret);
			pInputParams->listen_udp_port = 40000 + pInputParams->index;
			printf("+++libtssmooth now use port is %d \n",pInputParams->listen_udp_port);
		}
		else
		{
			printf("+++libtssmooth applyforUDPPort is %d ,ret = %d\n",pInputParams->listen_udp_port,ret);
		}
	}

	memset(p_instanse,0,sizeof(tssmooth_instanse));
	strncpy(p_instanse->listen_udp_ip,pInputParams->listen_udp_ip,sizeof(p_instanse->listen_udp_ip));
	p_instanse->listen_udp_port = pInputParams->listen_udp_port;
	strncpy(p_instanse->dst_udp_ip,pInputParams->dst_udp_ip,sizeof(p_instanse->dst_udp_ip));
	p_instanse->dst_udp_port = pInputParams->dst_udp_port;
	p_instanse->bit_rate = pInputParams->bit_rate;
	p_instanse->buffer_max_size = pInputParams->buffer_max_size;
	
	p_instanse->read_thread_status = 0;
	p_instanse->write_thread_status = 0;
	sem_init(&p_instanse->m_sem, 0, 0);
	p_instanse->real_send_bytes = 0;

	for(int i = 0;i < BUFFER_POOL_NUMBER;i++)
	{
		p_instanse->buffer_pool[i].buffer_size = p_instanse->buffer_max_size/BUFFER_POOL_NUMBER;
		p_instanse->buffer_pool[i].buffer_bytes = 0;
		pthread_mutex_init(&p_instanse->buffer_pool[i].m_mutex, 0);
		p_instanse->buffer_pool[i].data = (char *)malloc(p_instanse->buffer_pool[i].buffer_size);
		if(p_instanse->buffer_pool[i].data == NULL)
		{
			fprintf(stderr, "libtssmooth malloc buffer failed...p_instanse->buffer_pool[%d].data.\n",i);
			free(p_instanse);
			return NULL;
		}
	}

	pthread_attr_t attr;	
	pthread_attr_init(&attr);	
	pthread_attr_setstacksize(&attr,10*1024*1024);	
	
	if (pthread_create(&p_instanse->read_thread_id, &attr, &ts_recv_thread, p_instanse) != 0)		
	{
		fprintf(stderr, "libtssmooth pthread_create failed...\n");
		free(p_instanse);
		return NULL;
	}
	if (pthread_create(&p_instanse->write_thread_id, &attr, &ts_send_thread, p_instanse) != 0)		
	{
		fprintf(stderr, "libtssmooth pthread_create failed...\n");
		free(p_instanse);
		return NULL;
	}
	
	return p_instanse;
	
}

unsigned long get_real_send_bytes(ts_smooth_t *smooth)
{
	if(smooth == NULL)
	{		 
		//fprintf(stderr ,"libtssmooth: Error paraments..\n");		
		return -1;	  
	}
	tssmooth_instanse *p_instanse = (tssmooth_instanse *)smooth;

	
	return p_instanse->real_send_bytes;
}

int uninit_tssmooth(ts_smooth_t *smooth)
{
	if(smooth == NULL)
	{		 
		fprintf(stderr ,"libtssmooth: Error paraments..\n");		
		return -1;	  
	}
	tssmooth_instanse *p_instanse = (tssmooth_instanse *)smooth;

	pthread_cancel(p_instanse->read_thread_id);
	pthread_cancel(p_instanse->write_thread_id);
	
	for(int i = 0;i < BUFFER_POOL_NUMBER;i++)
	{
		pthread_mutex_destroy(&p_instanse->buffer_pool[0].m_mutex);
		if(p_instanse->buffer_pool[0].data != NULL)
			free(p_instanse->buffer_pool[0].data);
	}
	
	free(p_instanse);

	return 0;
}



