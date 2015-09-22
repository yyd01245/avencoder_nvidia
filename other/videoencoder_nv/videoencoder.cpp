#include "videoencoder.h"
#include "encodenv.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>


typedef struct{
    int flag;
    int x;
    int y;
    int w;
    int h;

}enc_rect_t;



typedef struct{
    
    unsigned char*input;
    unsigned char*output;
    unsigned char*prect;
    int output_siz;
    int flag_inc;
}nv_io_t;




typedef struct
{
	int width;
	int height;
	int gopsize_min;
	int gopsize_max;
	int bitrate;//bps
	int fps;

    int devid;//gpu core
//j	unsigned char *outbf;
//	unsigned char *enc_buf;
//	unsigned char *rgb_buffer;
//    unsigned char* rgb_shm;
    unsigned char* shm_addr;
    enc_rect_t shm_rect;
	//for statistics
	unsigned long real_count_fps;

    nv_encoder_struct nvencoder;
    nv_io_t nvio;
}videoencoder_instanse;

int first=1;
int nv_read(nv_encoder_struct*penc,void*input,size_t width,size_t height,size_t filestride,size_t buffstride)
{
/*    int nr=0;

    nv_io_t*pnvio=(nv_io_t*)nv_encoder_get_io_data(penc);

    enc_rect_t*prect=(enc_rect_t* )pnvio->prect;

    int x,y ,w,h;
    */
//    fprintf(stderr,"[x:%d,y:%d]{w:%d,h:%d}\t",prect->x,prect->y,prect->w,prect->h);
/*
    if(prect->w==0||prect->h==0 ||!pnvio->flag_inc ){
        x=0;
        y=0;
        w=width;
        h=height;
    }else{
*/
#if 0
    if(pnvio->flag_inc){
        x=prect->x&~1;
        y=prect->y&~1;
        w=(prect->w+1)&~1;
        h=(prect->h+1)&~1;
        if(first){
            fprintf(stderr,"ENCODE INC\n");
            first=0;
        }
    }else{
        x=0;
        y=0;
        w=width;
        h=height;

        if(first){
            fprintf(stderr,"ENCODE FULL\n");
            first=0;
        }
    }
/*
    }
*/
//    fprintf(stderr,"[x:%d,y:%d]{w:%d,h:%d}\r",x,y,w,h);
    _frame_conf_set_head(penc,x,y,w,h);
#endif

    return width*height*4;

}

int nv_write(nv_encoder_struct*penc,void*output,size_t framesize)
{
     
    int nr=0;
    nv_io_t*pnvio=(nv_io_t*)nv_encoder_get_io_data(penc);
    memcpy(pnvio->output,output,framesize);
    pnvio->output_siz=framesize;

    return framesize;

}


video_encoder_t* init_video_encoder(InputParams_videoencoder *pInputParams)
{
	if(pInputParams == NULL)
	{        
		fprintf(stderr ,"libvideoencoder: Error paraments..\n");        
		return NULL;    
	}

	
	videoencoder_instanse *p_instanse = (videoencoder_instanse *)calloc(1,sizeof(videoencoder_instanse));
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
    p_instanse->devid=pInputParams->devid;

    p_instanse->shm_addr=pInputParams->shm_addr;
//	p_instanse->outbf = (unsigned char *)malloc(p_instanse->width*p_instanse->height*2);
//	p_instanse->enc_buf = (unsigned char *)malloc(p_instanse->width*p_instanse->height*2);
//	p_instanse->rgb_buffer = (unsigned char *)malloc(p_instanse->width*p_instanse->height*4);
//	if(p_instanse->outbf == NULL)
//	{        
//		fprintf(stderr ,"libvideoencoder: Error malloc..\n");        
//		return NULL;    
//	}
/////////////////////////////////////////////////////////
//
    nv_encoder_config nvconfig;
//    nv_encoder_struct nvencoder;

    nv_encoder_get_default_config(&nvconfig);

    nvconfig.device_id=p_instanse->devid;
#if 0
    nvconfig.shm_addr=p_instanse->shm_addr+sizeof(enc_rect_t);

#else
    nvconfig.shm_addr=p_instanse->shm_addr;

#endif
    nvconfig.width=p_instanse->width;
    nvconfig.height=p_instanse->height;
    nvconfig.idr_period_startup=p_instanse->gopsize_min;
    nvconfig.gop_length=p_instanse->gopsize_max;
    //TODO
    nvconfig.frame_rate_num=p_instanse->fps;
    nvconfig.frame_rate_den=1;
    //

    nvconfig.conv_type=0;//0:cuda 1:host


	if(p_instanse->width<=720)
		p_instanse->bitrate=4000;
	else if(p_instanse->width<=1280 && p_instanse->width>720 )
		p_instanse->bitrate=4000;
	else if(p_instanse->width>1280)
		p_instanse->bitrate=9000;

    nvconfig.avg_bit_rate=p_instanse->bitrate*1024;
    nvconfig.peak_bit_rate=0;


	fprintf(stderr ,"libvideoencoder: Init H.264 encoder..\n");
    nv_encoder_init(&p_instanse->nvencoder,&nvconfig);

//    p_instanse->nvio.input=shm_addr;
    p_instanse->nvio.prect=p_instanse->shm_addr;

    nv_encoder_set_read(&p_instanse->nvencoder,nv_read,&p_instanse->nvio);
    nv_encoder_set_write(&p_instanse->nvencoder,nv_write,&p_instanse->nvio);

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

    int ret;

    p_instanse->nvio.output=(unsigned char*)output_video_sample;
    p_instanse->nvio.flag_inc=0;

    ret=nv_encoder_encode(&p_instanse->nvencoder,0);

//    usleep(100);

    *output_video_sample_size=p_instanse->nvio.output_siz;

	p_instanse->real_count_fps++;

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

    int ret;

    _frame_conf_set_head(&p_instanse->nvencoder,x,y,w,h);
    p_instanse->nvio.output=(unsigned char*)output_video_sample;
    p_instanse->nvio.flag_inc=1;

    ret=nv_encoder_encode(&p_instanse->nvencoder,0);

//    usleep(100);

    *output_video_sample_size=p_instanse->nvio.output_siz;

	//printf("x=%d,y=%d,w=%d,h=%d\n",x,y,w,h);
	p_instanse->real_count_fps++;
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



    nv_encoder_fini(&p_instanse->nvencoder);

//	free(p_instanse->outbf);
//	free(p_instanse->enc_buf);
	free(p_instanse);

//	close_encoder();


	return 0;
}




