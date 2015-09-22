#include "videoencoder.h"

extern "C" {

	#include "dm816x_encoder.h"
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>


#define POLL_TIME -1

typedef struct
{
	int width;
	int height;
	int gopsize_min;
	int gopsize_max;
	int bitrate;
	int fps;

	dm_venc_handle_t fd;
	dm816x_venc_params_t params;
	dm_venc_buff_info_t buff_info;

	struct pollfd fds;

	//for statistics
	unsigned long real_count_fps;
	
}videoencoder_instanse;


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
	
	memset(&p_instanse->params,0,sizeof(p_instanse->params));
	memset(&p_instanse->buff_info,0,sizeof(p_instanse->buff_info));
	
	p_instanse->width = pInputParams->width;
	p_instanse->height = pInputParams->height;
	p_instanse->gopsize_min = pInputParams->gopsize_min;
	p_instanse->gopsize_max = pInputParams->gopsize_max;
	p_instanse->bitrate = pInputParams->bitrate;
	p_instanse->fps = pInputParams->fps;
	
	p_instanse->params.width = p_instanse->width;
	p_instanse->params.height = p_instanse->height;
	p_instanse->params.dev_id = DM_DEV_ANY;
	p_instanse->params.chan_id = DM_DEV_ANY;
	p_instanse->params.codec = 0;
	p_instanse->params.bitrate = p_instanse->bitrate;
	p_instanse->params.gop_size = p_instanse->gopsize_min;
	p_instanse->real_count_fps = 0;

	ret = dm_venc_open(&p_instanse->fd);
	if(ret != 0)
	{		 
		fprintf(stderr ,"libvideoencoder: dm_venc_open error..\n"); 	   
		return NULL;	
	}

	ret = dm_venc_start_channel(p_instanse->fd,&p_instanse->params);//打开编码通道,params:源数据信息 
	if(ret != 0)
	{		 
		fprintf(stderr ,"libvideoencoder: dm_venc_start_channel error..\n"); 	   
		return NULL;	
	}

	ret = dm_venc_get_buff_info(p_instanse->fd,&p_instanse->buff_info);
	if(ret != 0)
	{
		fprintf(stderr ,"libvideoencoder: dm_venc_get_buff_info error..\n"); 	   
		return NULL;
	}  

	int fileno = dm_venc_get_fileno(p_instanse->fd);
	if(fileno < 0)
	{		 
		fprintf(stderr ,"libvideoencoder: dm_venc_get_fileno error..\n"); 	   
		return NULL;	
	}
	
	p_instanse->fds.fd = fileno;
	p_instanse->fds.events = POLLIN;
	p_instanse->fds.revents = 0;


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

	unsigned char *real_bits = NULL;
	int real_bits_size = 0;
	int ret = 0;
	dm816x_venc_frame_t frame;

	int yuv_buffer_size = input_video_sample_size;
	char *yuv_buffer = (char *)input_video_sample;

	int times = 0;
	while(1)
	{
		ret = dm_venc_is_enc_idle(p_instanse->fd);
		if(ret >= 0)
			break;
		else if(times >= 4)
			break;
		
		times++;
		usleep(5*1000);
	}
	
	if(ret < 0)
	{
		printf("libvideoencoder: dm_venc_is_enc_idle dm is not idle..dev_num=%d dev_id=%d chan_id=%d..\n",dm_venc_get_dev_num(p_instanse->fd),dm_venc_get_dev_id(p_instanse->fd),dm_venc_get_chan_id(p_instanse->fd));
		return -3; 
	}
	
	//printf("x=%d,y=%d,w=%d,h=%d\n",x,y,w,h);

	
	for(int i = 0;i <h;i++)
	{
		memcpy(p_instanse->buff_info.data0  +  p_instanse->width*(y + i)  + x,yuv_buffer + i * w,w);
		if(i%2 == 0)
			memcpy(p_instanse->buff_info.data1+p_instanse->width*(y + i)/2+ x,yuv_buffer+w*h+w*i/2,w);
	}

	/*
	unsigned char *dst[2];
	unsigned char *src[2];
	int pitch[2];

	pitch[0] = p_instanse->width;
	pitch[1] = p_instanse->width/2;
	dst[0] = p_instanse->buff_info.data0 +  pitch[0] * y + x;
	dst[1] = p_instanse->buff_info.data1 +  pitch[1] * y + x;
	src[0] = (unsigned char *)yuv_buffer;
	src[1] = (unsigned char *)yuv_buffer +  w*h;

	for(int i=0;i<h;i++)
	{
		for(int j=0; j<w;j++)
		{
			dst[0][j]=src[0][j];
		}
			dst[0]+=pitch[0];
			src[0]+=w;
	}

	for(int i=0;i<h;i+=2)
	{
		for(int j=0; j<w;j++)
		{
			dst[1][j]=src[1][j];   
		}
			dst[1]+=pitch[1];
			src[1]+=w;
	}
	*/
	
	memset(&frame,0,sizeof(frame));
	frame.data.active_x1=x;
	frame.data.active_y1=y;
	frame.data.active_x2 = x+w;	 //表示源数据动态变化坐标
	frame.data.active_y2 = y+h;	//
	
	ret = dm_venc_write_data(p_instanse->fd, &frame.data);//包含源数据变化区域
	if(ret != 0)
	{
		fprintf(stderr ,"libvideoencoder: dm_venc_write_data error..\n");
		return -4;
	} 

	ret = poll(&p_instanse->fds, 1, POLL_TIME);
	if(ret > 0)
	{
		ret = dm_venc_read_bits(p_instanse->fd, &frame.bits);
		if(ret != 0)
		{
			fprintf(stderr ,"libvideoencoder: dm_venc_read_bits error..\n");
			return -5;
		} 
		real_bits = p_instanse->buff_info.bits- frame.bits.bits_offset;  //这个包含编码后数据指指针
	}
	else
	{
		fprintf(stderr ,"libvideoencoder: dm_venc_read_bits poll error..\n");
		return -6;
	}

	real_bits_size = frame.bits.bits_size;
	
	p_instanse->real_count_fps++;
	if(p_instanse->real_count_fps == 5*p_instanse->fps)//10s
	{
		p_instanse->params.gop_size = p_instanse->gopsize_max;
		dm_venc_change_encode_param(p_instanse->fd,&p_instanse->params);
	}

	if(*output_video_sample_size < real_bits_size)
	{		 
		fprintf(stderr ,"libvideosource: Error output_video_sample_size is too small..\n");
		return -7;	  
	}

	memcpy(output_video_sample,real_bits,real_bits_size);
	*output_video_sample_size = real_bits_size;
	
	return 0;
}

int uninit_video_encoder(video_encoder_t *videoencoder)
{
	if(videoencoder == NULL)
	{        
		fprintf(stderr ,"libaudioencoder: Error paraments..\n");        
		return -1;    
	}
	videoencoder_instanse *p_instanse = (videoencoder_instanse *)videoencoder;

	dm_venc_close(p_instanse->fd);

	free(p_instanse);

	return 0;
}


