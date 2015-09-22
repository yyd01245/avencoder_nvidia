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
	
	#include "libswscale/swscale.h"
	#include "libavcodec/avcodec.h"

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
	
	char *yuv_buffer;
	int yuv_buffer_size;

	//for statistics
	unsigned long real_count_fps;
	
}videoencoder_instanse;

static int BGRA_to_YUV420SP(int width,int height,const char *pSrc,int iSrc,char *pDst,int *iDst)
{
	if(pSrc == NULL || pDst == NULL || iDst == NULL)
	{        
		fprintf(stderr ,"libaudioencoder: Error paraments..\n");        
		return -1;    
	}
	
	/*
	//fill picture
	AVPicture picture_src;
	AVPicture picture_dst;
	int ret1 = avpicture_fill(&picture_src, (uint8_t*)pSrc,PIX_FMT_RGB32,width, height);
	int ret2 = avpicture_fill(&picture_dst, (uint8_t*)pDst,PIX_FMT_NV12,width, height);
	if (ret1 < 0 || ret2 < 0)	 
	{ 
		fprintf(stderr ,"libvideoencoder: ffmpeg avpicture_fill error..\n");
		return -2;	
	}

	SwsContext* m_pSwsContext = sws_getContext(width, height, PIX_FMT_RGB32,width,height, PIX_FMT_NV12,SWS_BICUBIC,NULL, NULL, NULL); 							
	if (NULL == m_pSwsContext)	 
	{ 
		fprintf(stderr ,"libvideoencoder: ffmpeg sws_getContext error..\n");
		return -3;	
	}
	
	sws_scale(m_pSwsContext, picture_src.data,picture_src.linesize, 0,  height,picture_dst.data, picture_dst.linesize);  
            
	sws_freeContext(m_pSwsContext);
	*/

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

	p_instanse->yuv_buffer_size = p_instanse->width*p_instanse->height*3/2;

	p_instanse->yuv_buffer = (char *)malloc(p_instanse->yuv_buffer_size);
	if(p_instanse->yuv_buffer == NULL)
	{
		fprintf(stderr ,"libvideoencoder: malloc fail..\n");        
		return NULL;
	}

	return p_instanse;
}

int encode_video_sample(video_encoder_t *videoencoder,const char *input_video_sample,int input_video_sample_size,
	char *output_video_sample,int *output_video_sample_size)
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
	
	int yuv_buffer_size = p_instanse->yuv_buffer_size;
	char *yuv_buffer = p_instanse->yuv_buffer;
	//memset(yuv_buffer,0,yuv_buffer_size);

	ret = BGRA_to_YUV420SP(p_instanse->width,p_instanse->height,input_video_sample,input_video_sample_size,yuv_buffer,&yuv_buffer_size);
	if(ret < 0)
	{
		fprintf(stderr ,"libvideoencoder: BGRA_to_YUV420P error ret=%d..\n",ret);
		return -2;
	}

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
	
	memcpy(p_instanse->buff_info.data0,yuv_buffer,p_instanse->buff_info.pitch0 * p_instanse->buff_info.max_height); //复制要编码源数据Y分量
	memcpy(p_instanse->buff_info.data1,yuv_buffer+p_instanse->width*p_instanse->height,p_instanse->buff_info.pitch1 * p_instanse->buff_info.max_height/2);//复制要编码源数据UV分量
	memset(&frame,0,sizeof(frame));
	frame.data.active_x1=0;
	frame.data.active_y1=0;
	frame.data.active_x2 = p_instanse->buff_info.pitch0-1;   //表示源数据动态变化坐标
	frame.data.active_y2 = p_instanse->buff_info.max_height-1;  //
	
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

	int padding = 2048 - real_bits_size;
	if(padding < 0)
		padding = 0;
	
	p_instanse->real_count_fps++;
	if(p_instanse->real_count_fps == 5*p_instanse->fps)//10s
	{
		p_instanse->params.gop_size = p_instanse->gopsize_max;
		dm_venc_change_encode_param(p_instanse->fd,&p_instanse->params);
	}

	if(*output_video_sample_size < real_bits_size + padding)
	{		 
		fprintf(stderr ,"libvideosource: Error output_video_sample_size is too small..\n");
		return -7;	  
	}

	memcpy(output_video_sample,real_bits,real_bits_size);
	memset(output_video_sample + real_bits_size,0,padding);
	*output_video_sample_size = real_bits_size + padding;
	
	return 0;
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

	int yuv_buffer_size = p_instanse->yuv_buffer_size;
	char *yuv_buffer = p_instanse->yuv_buffer;
	//memset(yuv_buffer,0,yuv_buffer_size);
	
	ret = BGRA_to_YUV420SP(w,h,input_video_sample,input_video_sample_size,yuv_buffer,&yuv_buffer_size);
	if(ret < 0)
	{
		fprintf(stderr ,"libvideoencoder: BGRA_to_YUV420P error ret=%d..\n",ret);
		return -2;
	}

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
	
	int padding = 2048 - real_bits_size;
	if(padding < 0)
		padding = 0;
	
	p_instanse->real_count_fps++;
	if(p_instanse->real_count_fps == 5*p_instanse->fps)//10s
	{
		p_instanse->params.gop_size = p_instanse->gopsize_max;
		dm_venc_change_encode_param(p_instanse->fd,&p_instanse->params);
	}

	if(*output_video_sample_size < real_bits_size + padding)
	{		 
		fprintf(stderr ,"libvideosource: Error output_video_sample_size is too small..\n");
		return -7;	  
	}

	memcpy(output_video_sample,real_bits,real_bits_size);
	memset(output_video_sample + real_bits_size,0,padding);
	*output_video_sample_size = real_bits_size + padding;
	
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

	dm_venc_close(p_instanse->fd);

	if(p_instanse->yuv_buffer != NULL)
		free(p_instanse->yuv_buffer);
	free(p_instanse);

	return 0;
}


