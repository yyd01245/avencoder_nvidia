#include "videoencoder.h"

#include <pthread.h>


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>

#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>  
#include <netinet/tcp.h>

typedef struct
{
	int type;
	int length;
	int x;
	int y;
	int w;
	int h;
}msg_head;

int width = 1280;
int height = 720;
int encode_bitrate = 3*1024*1024;
int gopsize_min = 5;
int gopsize_max = 100;
int fps = 25;

int recv_len(int sock,char *buffer,int buffer_len,int req_len)
{
	if(req_len == 0)
		return 0;
	
	if(buffer == NULL || buffer_len < 0 || req_len < 0 || req_len > buffer_len)
		return -1;
	
	int i_req_len = req_len;
	int i_count = -1;
	int sum = 0;
	while(1)
	{
		i_count = recv(sock,buffer+sum,i_req_len-sum,0);
		if(i_count > 0) 
		{

		}
		else if (i_count == -1)
		{
			if(errno!= EAGAIN)
			{
				//return -1;
			}
			break;
		}
		else
			break;
		//return -1;
		sum += i_count;

		if (sum >= i_req_len) 
			break;
	}

	return sum;
}

void* process_videoencoder(void *param)
{
	int sock = *(int *)param;
	free(param);
	printf("videoencoder new session sock=%d\n",sock);

	video_encoder_t* p_video_encoder = NULL;
	InputParams_videoencoder video_encoder_params;
	video_encoder_params.width = width;
	video_encoder_params.height = height;
	video_encoder_params.gopsize_min = gopsize_min;
	video_encoder_params.gopsize_max = gopsize_max;
	video_encoder_params.bitrate = encode_bitrate;
	video_encoder_params.fps = fps;
	p_video_encoder = init_video_encoder(&video_encoder_params);
	if(p_video_encoder == NULL)
	{		 
		printf("videoencoder: init_video_encoder fail..\n");		 
		return NULL;	  
	}


	

	
	int buffer_size = width*height*3/2+24;
	char *buffer = (char *)malloc(buffer_size);
	if(buffer == NULL)
	{		 
		printf("videoencoder malloc buffer fail..\n");		 
		return NULL;	  
	}
	
	char *input_video_sample;
	int input_video_sample_size;
	
	int output_video_sample_size = width*height*3/2;
	char *output_video_sample = (char *)malloc(buffer_size);
	if(output_video_sample == NULL)
	{		 
		fprintf(stderr ,"malloc buffer fail..\n");		 
		return NULL;	  
	}

	int len = -1;
	int ret = -1;

	int type;
	int length;
	int x;
	int y;
	int w;
	int h;
	msg_head head;

	struct timeval tv;
	long time_us1,time_us2,time_us3,time_us4;


	while (1)
	{
		gettimeofday(&tv,NULL);
		time_us1 = tv.tv_sec*1000 + tv.tv_usec/1000;
		printf("time_us1 =%ld start recv\n",time_us1);
		
		len = recv(sock,&head,sizeof(head),0);
		if(len == 0)
		{
			fprintf(stderr ,"connect broken error len=%d..\n",len);
			break;
		}
		
		if(len < 0)
		{
			fprintf(stderr ,"recv data error len=%d..\n",len);
			continue;
		}

		type = head.type;
		length = head.length;
		x=head.x;
		y=head.y;
		w=head.w;
		h=head.h;

		if(length != 16+w*h*3/2)
		{
			fprintf(stderr ,"length error len=%d..\n",len);
			continue;
		}

		len = recv_len(sock,buffer,buffer_size,w*h*3/2);
		if(len != w*h*3/2)
		{
			fprintf(stderr ,"recv data error len=%d..\n",len);
			continue;
		}

		input_video_sample = buffer;
		input_video_sample_size = w * h *3/2;

		gettimeofday(&tv,NULL);
		time_us2 = tv.tv_sec*1000 + tv.tv_usec/1000;
		printf("time_us2 =%ld type=%d length=%d x=%d y=%d w=%d h=%d\n",time_us2,head.type,head.length,head.x,head.y,head.w,head.h);


		output_video_sample_size = width*height*3/2;
		ret = encode_video_sample_inc(p_video_encoder,input_video_sample,input_video_sample_size,
	output_video_sample,&output_video_sample_size,x,y,w,h);
		if(ret < 0)
		{
			fprintf(stderr ,"encode_video_sample_inc error ret=%d..\n",ret);
			continue;
		}

		gettimeofday(&tv,NULL);
		time_us3 = tv.tv_sec*1000 + tv.tv_usec/1000;
		printf("time_us3 =%ld encode_video_sample length=%d\n",time_us3,output_video_sample_size);

		head.type = 2;
		head.length = 4*4 + output_video_sample_size;
		head.x = 0;
		head.y = 0;
		head.w = 0;
		head.h = 0;

		len = send(sock,&head,sizeof(head),0);
		if(len < 0)
		{
			fprintf(stderr ,"send head error ret=%d..\n",len);
			continue;
		}

		len = send(sock,output_video_sample,output_video_sample_size,0);
		if(len < 0)
		{
			fprintf(stderr ,"send output_video_sample error ret=%d..\n",len);
			continue;
		}

		gettimeofday(&tv,NULL);
		time_us4 = tv.tv_sec*1000 + tv.tv_usec/1000;
		printf("time_us4 =%ld send over\n",time_us4);
		
	}

	uninit_video_encoder(p_video_encoder);
	
}

int main()
{
	//ÆÁ±ÎÒ»Ð©³£¼ûÐÅºÅ£¬·ÀÖ¹³ÌÐòÒì³£ÖÕÖ¹
	signal(SIGINT, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGFPE, SIG_IGN);
	signal(SIGSEGV, SIG_IGN); 

	//deamon
	if(fork())
		return 0;
	
	int listen_port = 11111;
	//´´½¨¼àÌýÌ×½Ó×Ö
    int listener;
    listener = socket(AF_INET, SOCK_STREAM, 0);
    if(listener <= 0)
	{
		perror("create socket error");
		return -1;
    }

	//ÉèÖÃ¼àÌýÌ×½Ó×ÖÊôÐÔ

	int optval = 1;
    setsockopt(listener,IPPROTO_TCP,TCP_NODELAY,(char *)(&optval),sizeof(optval));
	setsockopt(listener,SOL_SOCKET,SO_KEEPALIVE,(char *)(&optval),sizeof(optval));
	setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,(char *)(&optval),sizeof(optval));

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = 0;
    sin.sin_port = htons(listen_port);
	//°ó¶¨
    if (bind(listener, (struct sockaddr *)&sin, sizeof(sin)) < 0) 
	{
        perror("bind error");
		close(listener);
        return -2;
    }
	//¼àÌý
    if (listen(listener, 5) < 0) 
	{
        perror("listen error");
		close(listener);
        return -3;
    }

	struct sockaddr_in cli_addr;
	int newfd;
	socklen_t sin_size;
	sin_size = sizeof(struct sockaddr_in);
	while(1)
	{
		newfd = accept(listener, (struct sockaddr*)&cli_addr, &sin_size);
		if(newfd <= 0)
			continue;

		int *fd = (int *)malloc(sizeof(int));
		*fd = newfd;
		
		pthread_t pthread;
		int ret = pthread_create(&pthread, NULL, process_videoencoder, (void *)fd);
		if(ret != 0)
		{
			printf("pthread_create fail!...\n");
			continue;
		}
	}
}

