#include "videoencoder.h"

#ifndef INT64_C
#define INT64_C(c) c##LL
#endif
#ifndef UINT64_C
#define UINT64_C(c) c##LL
#endif

#include <stdint.h>
extern "C" {
	#include <ippcc.h>
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
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

typedef struct
{
	int width;
	int height;
	int gopsize_min;
	int gopsize_max;
	int bitrate;
	int fps;

	int sock;
	
	char *yuv_buffer;
	int yuv_buffer_size;

	char *es_buffer;
	int es_buffer_size;

	//for statistics
	unsigned long real_count_fps;
	
}videoencoder_instanse;

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


static int BGRA_to_YUV420SP(int width,int height,const char *pSrc,int iSrc,char *pDst,int *iDst)
{
	if(pSrc == NULL || pDst == NULL || iDst == NULL)
	{        
		fprintf(stderr ,"libaudioencoder: Error paraments..\n");        
		return -1;    
	}

	unsigned char *tmp_buffer = NULL;
	int tmp_buffer_size = width*height*3/2;

	tmp_buffer = (unsigned char *)malloc(tmp_buffer_size);
	if(tmp_buffer == NULL)
	{
		fprintf(stderr ,"libvideoencoder: malloc fail..\n");        
		return -2;
	}
	memset(tmp_buffer,0,tmp_buffer_size);
	
	
	int pDstStep[3];
	unsigned char *pDst2[3];
	pDstStep[0]=width;
	pDstStep[1]=width/2;
	pDstStep[2]=width/2;

	IppiSize roiSize;
	roiSize.width=width;
	roiSize.height=height;

	pDst2[0]=tmp_buffer;
	pDst2[1]=tmp_buffer+width*height;
	pDst2[2]=tmp_buffer+width*height+width*height/4;
	ippiBGRToYCbCr420_709CSC_8u_AC4P3R((unsigned char*)pSrc,width*4,pDst2,pDstStep,roiSize);

	memcpy(pDst,pDst2[0],width*height);
	
	for(int i = 0;i<width*height*1/4;i++)
	{
		pDst[2*i  + width*height] = pDst2[1][i];
		pDst[2*i+1+ width*height] = pDst2[2][i];
	}
	
	free(tmp_buffer);
	
	return 0;
}


video_encoder_t* init_video_encoder(InputParams_videoencoder *pInputParams)
{
	int ret = -1;
	if(pInputParams == NULL)
	{        
		fprintf(stderr ,"libvideoencoder: Error paraments..\n");        
		return NULL;    
	}


	videoencoder_instanse *p_instanse = (videoencoder_instanse *)malloc(sizeof(videoencoder_instanse));
	if(p_instanse == NULL)
	{        
		fprintf(stderr ,"libvideoencoder: malloc videoencoder_instanse error..\n");        
		return NULL;    
	}

	memset(p_instanse,0,sizeof(videoencoder_instanse));
	
	p_instanse->width = pInputParams->width;
	p_instanse->height = pInputParams->height;
	p_instanse->gopsize_min = pInputParams->gopsize_min;
	p_instanse->gopsize_max = pInputParams->gopsize_max;
	p_instanse->bitrate = pInputParams->bitrate;
	p_instanse->fps = pInputParams->fps;
	
	p_instanse->yuv_buffer_size = p_instanse->width*p_instanse->height*3/2;

	p_instanse->yuv_buffer = (char *)malloc(p_instanse->yuv_buffer_size);
	if(p_instanse->yuv_buffer == NULL)
	{
		fprintf(stderr ,"libvideoencoder: malloc fail..\n");        
		return NULL;
	}

	p_instanse->es_buffer_size = p_instanse->width*p_instanse->height*3/2;
	p_instanse->es_buffer = (char *)malloc(p_instanse->es_buffer_size);
	if(p_instanse->es_buffer == NULL)
	{
		fprintf(stderr ,"libvideoencoder: malloc fail..\n");        
		return NULL;
	}

	int sock;
	if((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		fprintf(stderr ,"Create sock connect to orderhub error,errno = %d \n", errno);
		return NULL;
	}

	int optval = 1;
    setsockopt(sock,IPPROTO_TCP,TCP_NODELAY,(char *)(&optval),sizeof(optval));
	setsockopt(sock,SOL_SOCKET,SO_KEEPALIVE,(char *)(&optval),sizeof(optval));
	setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(char *)(&optval),sizeof(optval));
	

	struct sockaddr_in servaddr;
	memset(&servaddr,0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr("192.168.200.200");;
	servaddr.sin_port = htons(11111);

	if (connect(sock, (struct sockaddr *)&servaddr, sizeof(struct sockaddr)) < 0)
	{
		fprintf(stderr ,"connect server error,errno = %d \n", errno);
		close(sock);
		return NULL;
	}
	
	p_instanse->sock = sock;

	return p_instanse;
}

int encode_video_sample_inc(video_encoder_t *videoencoder,const char *input_video_sample,int input_video_sample_size,
	char *output_video_sample,int *output_video_sample_size,int x,int y,int w,int h)
{
	if(videoencoder == NULL || input_video_sample == NULL || output_video_sample == NULL || output_video_sample_size == NULL)
	{		 
		fprintf(stderr ,"libvideoencoder: Error paraments..\n");		
		return -1;	  
	}
	
	videoencoder_instanse *p_instanse = (videoencoder_instanse *)videoencoder;

	int sock = p_instanse->sock;
	char *real_bits = p_instanse->es_buffer;
	int real_bits_size = p_instanse->es_buffer_size;
	int real_bits_length = 0;
	msg_head send_head;
	int ret = 0;

	int yuv_buffer_size = p_instanse->yuv_buffer_size;
	char *yuv_buffer = p_instanse->yuv_buffer;
	//memset(yuv_buffer,0,yuv_buffer_size);
	
	ret = BGRA_to_YUV420SP(w,h,input_video_sample,input_video_sample_size,yuv_buffer,&yuv_buffer_size);
	if(ret < 0)
	{
		fprintf(stderr ,"libvideoencoder: BGRA_to_YUV420P error ret=%d..\n",ret);
		return -2;
	}

	struct timeval tv;
	long time_us1,time_us2,time_us3,time_us4;

	send_head.type = 1;
	send_head.length = 4*4 + w*h*3/2;
	send_head.x = x;
	send_head.y = y;
	send_head.w = w;
	send_head.h = h;

	//gettimeofday(&tv,NULL);
	//time_us1 = tv.tv_sec*1000 + tv.tv_usec/1000;
	//printf("time_us1 =%ld start send\n",time_us1);
		
	ret = send(sock,&send_head,sizeof(send_head),0);
	if(ret < 0)
	{
		fprintf(stderr ,"libvideoencoder: send head error ret=%d..\n",ret);
		return -2;
	}
	ret = send(sock,yuv_buffer,w*h*3/2,0);
	if(ret < 0)
	{
		fprintf(stderr ,"libvideoencoder: send yuv error ret=%d..\n",ret);
		return -2;
	}
	
	//gettimeofday(&tv,NULL);
	//time_us2 = tv.tv_sec*1000 + tv.tv_usec/1000;
	//printf("time_us2 =%ld send over\n",time_us2);


	int type;
	int length;
	msg_head head;
	int len = recv(sock,&head,sizeof(head),0);
	if(len == 0)
	{
		fprintf(stderr ,"connect broken error ret=%d..\n",len);
		return -2;
	}
	
	if(len < 0)
	{
		fprintf(stderr ,"recv data error ret=%d..\n",len);
		return -2;
	}

	type = head.type;
	length = head.length;

	if(length - 16 < 0)
	{
		fprintf(stderr ,"recv data error ret=%d..\n",len);
		return -2;
	}

	len = recv_len(sock,real_bits,real_bits_size,length - 16);
	if(len != length - 16)
	{
		fprintf(stderr ,"recv data error len=%d..\n",len);
		return -2;
	}
	real_bits_length = len;

	
	//gettimeofday(&tv,NULL);
	//time_us3 = tv.tv_sec*1000 + tv.tv_usec/1000;
	//printf("time_us3 =%ld recv over\n",time_us3);
	
	
	int padding = 512 - real_bits_length;
	if(padding < 0)
		padding = 0;
	
	p_instanse->real_count_fps++;

	if(*output_video_sample_size < real_bits_length + padding)
	{		 
		fprintf(stderr ,"libvideosource: Error output_video_sample_size is too small..\n");
		return -7;	  
	}

	memcpy(output_video_sample,real_bits,real_bits_length);
	memset(output_video_sample + real_bits_length,0,padding);
	*output_video_sample_size = real_bits_length + padding;
	
	return 0;
}

unsigned long get_real_count_fps(video_encoder_t *videoencoder)
{
	if(videoencoder == NULL)
	{		 
		fprintf(stderr ,"libaudioencoder: Error paraments..\n");		
		return -1;	  
	}
	videoencoder_instanse *p_instanse = (videoencoder_instanse *)videoencoder;
	return p_instanse->real_count_fps;
}

int uninit_video_encoder(video_encoder_t *videoencoder)
{
	if(videoencoder == NULL)
	{        
		fprintf(stderr ,"libaudioencoder: Error paraments..\n");        
		return -1;    
	}
	videoencoder_instanse *p_instanse = (videoencoder_instanse *)videoencoder;


	if(p_instanse->yuv_buffer != NULL)
		free(p_instanse->yuv_buffer);
	free(p_instanse);

	return 0;
}


