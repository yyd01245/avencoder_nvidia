#include "videoencoder.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

	#include "vaenclib.h"
	#include <ippcc.h>

#ifdef __cplusplus
}
#endif

typedef struct
{
	int width;
	int height;
	int gopsize_min;
	int gopsize_max;
	int bitrate;//bps
	int fps;

	unsigned char *outbf;
	unsigned char *enc_buf;
	unsigned char *rgb_buffer;

	//for statistics
	unsigned long real_count_fps;
}videoencoder_instanse;


video_encoder_t* init_video_encoder(InputParams_videoencoder *pInputParams)
{
	if(pInputParams == NULL)
	{        
		fprintf(stderr ,"libvideoencoder: Error paraments..\n");        
		return NULL;    
	}

	
	videoencoder_instanse *p_instanse = (videoencoder_instanse *)malloc(sizeof(videoencoder_instanse));
	if(p_instanse == NULL)
	{        
		fprintf(stderr ,"libvideoencoder: Error malloc..\n");        
		return NULL;    
	}

	p_instanse->width = pInputParams->width;
	p_instanse->height = pInputParams->height;
	p_instanse->gopsize_min = pInputParams->gopsize_min;
	p_instanse->gopsize_max = pInputParams->gopsize_max;
	p_instanse->fps = pInputParams->fps;
	p_instanse->bitrate = pInputParams->bitrate/1024;
	p_instanse->real_count_fps = 0;
	
	p_instanse->outbf = (unsigned char *)malloc(p_instanse->width*p_instanse->height*2);
	p_instanse->enc_buf = (unsigned char *)malloc(p_instanse->width*p_instanse->height*2);
	p_instanse->rgb_buffer = (unsigned char *)malloc(p_instanse->width*p_instanse->height*4);
	if(p_instanse->outbf == NULL || p_instanse->enc_buf == NULL || p_instanse->rgb_buffer == NULL)
	{        
		fprintf(stderr ,"libvideoencoder: Error malloc..\n");        
		return NULL;    
	}

	unsigned char parn=0;
	char mkarg[30][30];
	char * parg[30];
	sprintf(mkarg[parn],"main\0");
	parg[parn]=mkarg[parn];
	parn++;
	sprintf(mkarg[parn],"%d\0",p_instanse->width);
	parg[parn]=mkarg[parn];
	parn++;
	sprintf(mkarg[parn],"%d\0",p_instanse->height);
	parg[parn]=mkarg[parn];
	parn++;

	if(p_instanse->width<=720)
		p_instanse->bitrate=4000;
	else if(p_instanse->width<=1280 && p_instanse->width>720 )
		p_instanse->bitrate=4000;
	else if(p_instanse->width>1280)
		p_instanse->bitrate=9000;

	sprintf(mkarg[parn],"fb=%d\0",p_instanse->bitrate);
	parg[parn]=mkarg[parn];
	parn++;
	sprintf(mkarg[parn],"mode=%d\0",1);
	parg[parn]=mkarg[parn];
	parn++;

	//fprintf(stderr ,"libvideoencoder: Init H.264 encoder..\n");
	if(init_encoder(parn,parg) != 0)
	{
		fprintf(stderr ,"libvideoencoder: Init H.264 encoder error..\n");
		return NULL;
	}
	
	//fprintf(stderr ,"libvideoencoder: Init H.264 encoder success with w=%d,h=%d\n",p_instanse->width,p_instanse->height);

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

	int width = p_instanse->width;
	int height = p_instanse->height;
	
	unsigned char* outbf = p_instanse->outbf;
	if(outbf == NULL)
	{        
		fprintf(stderr ,"libvideoencoder: Error malloc ..\n");        
		return -2;    
	}
	
	int pDstStep[3];
	unsigned char *pDst[3];
	pDstStep[0]=width;
	pDstStep[1]=width/2;
	pDstStep[2]=width/2;

	IppiSize roiSize;
	roiSize.width=width;
	roiSize.height=height;

	pDst[0]=outbf;
	pDst[1]=outbf+width*height;
	pDst[2]=outbf+width*height+width*height/4;
	ippiBGRToYCbCr420_709CSC_8u_AC4P3R((unsigned char*)input_video_sample,width*4,pDst,pDstStep,roiSize);

	int enc_size,offset;
	unsigned char *enc_buf=p_instanse->enc_buf;
	if(enc_buf == NULL)
	{        
		fprintf(stderr ,"libvideoencoder: Error malloc ..\n");        
		return -2;    
	}
	
	struct coded_buff cdbf;
	cdbf.buff=enc_buf;
	
	encode_frame(outbf,&cdbf);

	int padding = 2048 - cdbf.length;
	if(padding < 0)
		padding = 0;

	p_instanse->real_count_fps++;

	if(*output_video_sample_size < cdbf.length + padding)
	{		 
		fprintf(stderr ,"libvideoencoder: Error output_video_sample_size is too small..\n");		
		return -3;	  
	} 
	
	memcpy(output_video_sample,cdbf.buff,cdbf.length);
	memset(output_video_sample + cdbf.length,0,padding);
	*output_video_sample_size = cdbf.length + padding;

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

	int width = p_instanse->width;
	int height = p_instanse->height;

	//printf("x=%d,y=%d,w=%d,h=%d\n",x,y,w,h);

	unsigned char* rgb_buffer = p_instanse->rgb_buffer;
	if(rgb_buffer == NULL)
	{        
		fprintf(stderr ,"libvideoencoder: Error malloc ..\n");        
		return -2;    
	}

	//copy outbf->rgb_buffer
	
	for(int i = 0;i < h;i++)
		memcpy(rgb_buffer + width * (i+ y) * 4 + x * 4,input_video_sample + w *i * 4,w * 4);

	
	unsigned char* outbf = p_instanse->outbf;
	if(outbf == NULL)
	{        
		fprintf(stderr ,"libvideoencoder: Error malloc ..\n");        
		return -2;    
	}
	
	int pDstStep[3];
	unsigned char *pDst[3];
	pDstStep[0]=width;
	pDstStep[1]=width/2;
	pDstStep[2]=width/2;

	IppiSize roiSize;
	roiSize.width=width;
	roiSize.height=height;

	pDst[0]=outbf;
	pDst[1]=outbf+width*height;
	pDst[2]=outbf+width*height+width*height/4;
	ippiBGRToYCbCr420_709CSC_8u_AC4P3R((unsigned char*)rgb_buffer,width*4,pDst,pDstStep,roiSize);

	int enc_size,offset;
	unsigned char *enc_buf=p_instanse->enc_buf;
	if(enc_buf == NULL)
	{        
		fprintf(stderr ,"libvideoencoder: Error malloc ..\n");        
		return -2;    
	}
	
	struct coded_buff cdbf;
	cdbf.buff=enc_buf;
	
	encode_frame(outbf,&cdbf);

	int padding = 2048 - cdbf.length;
	if(padding < 0)
		padding = 0;

	p_instanse->real_count_fps++;

	if(*output_video_sample_size < cdbf.length + padding)
	{		 
		fprintf(stderr ,"libvideoencoder: Error output_video_sample_size is too small..\n");		
		return -3;	  
	} 
	
	memcpy(output_video_sample,cdbf.buff,cdbf.length);
	memset(output_video_sample + cdbf.length,0,padding);
	*output_video_sample_size = cdbf.length + padding;

	return 0;
}

unsigned long get_real_count_fps(video_encoder_t *videoencoder)
{
	if(videoencoder == NULL)
	{		 
		fprintf(stderr ,"libvideoencoder: Error paraments..\n");		
		return -1;	  
	}
	videoencoder_instanse *p_instanse = (videoencoder_instanse *)videoencoder;
	return p_instanse->real_count_fps;
}

int uninit_video_encoder(video_encoder_t *videoencoder)
{
	if(videoencoder == NULL)
	{		 
		fprintf(stderr ,"libvideoencoder: Error paraments..\n");		
		return -1;	  
	}
	videoencoder_instanse *p_instanse = (videoencoder_instanse *)videoencoder;

	free(p_instanse->outbf);
	free(p_instanse->enc_buf);
	free(p_instanse);

	close_encoder();

	return 0;
}




