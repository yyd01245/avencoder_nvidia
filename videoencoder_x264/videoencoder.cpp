#include "videoencoder.h"

#ifndef INT64_C
#define INT64_C(c) c##LL
#endif
#ifndef UINT64_C
#define UINT64_C(c) c##LL
#endif

#include <stdint.h>
extern "C" {
	#include <x264.h>
	#include "libswscale/swscale.h"
	#include "libavcodec/avcodec.h"
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if 0
from x264.h
#define X264_CSP_NONE           0x0000  /* Invalid mode     */
#define X264_CSP_I420           0x0001  /* yuv 4:2:0 planar */
#define X264_CSP_YV12           0x0002  /* yvu 4:2:0 planar */
#define X264_CSP_NV12           0x0003  /* yuv 4:2:0, with one y plane and one packed u+v */
#define X264_CSP_I422           0x0004  /* yuv 4:2:2 planar */
#define X264_CSP_YV16           0x0005  /* yvu 4:2:2 planar */
#define X264_CSP_NV16           0x0006  /* yuv 4:2:2, with one y plane and one packed u+v */
#define X264_CSP_V210           0x0007  /* 10-bit yuv 4:2:2 packed in 32 */
#define X264_CSP_I444           0x0008  /* yuv 4:4:4 planar */
#define X264_CSP_YV24           0x0009  /* yvu 4:4:4 planar */
#define X264_CSP_BGR            0x000a  /* packed bgr 24bits   */
#define X264_CSP_BGRA           0x000b  /* packed bgr 32bits   */
#define X264_CSP_RGB            0x000c  /* packed rgb 24bits   */
#endif

#define ENCODER_PRESET "veryfast"
#define ENCODER_TUNE   "zerolatency"
#define ENCODER_PROFILE  "baseline"
#define ENCODER_COLORSPACE X264_CSP_NV12

typedef struct
{
	int width;
	int height;
	int gopsize_min;
	int gopsize_max;
	int bitrate;//bps
	int fps;
	
	x264_param_t* 	x264_parameter;
	x264_t* 		x264_encoder;

	long i_pts;
	
	char parameter_preset[32];
	char parameter_tune[32];
	char parameter_profile[32];
	long colorspace;

	unsigned char *rgb_buffer;
	int rgb_buffer_size;
	unsigned char *yuv_buffer;
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
	if(width == 0 || height == 0)
	{
		return 0;
	}
	

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
	p_instanse->i_pts = 0;

	p_instanse->yuv_buffer_size = p_instanse->width*p_instanse->height*3/2;
	p_instanse->rgb_buffer_size = p_instanse->width*p_instanse->height*4;
	p_instanse->yuv_buffer = (unsigned char *)malloc(p_instanse->yuv_buffer_size);
	p_instanse->rgb_buffer = (unsigned char *)malloc(p_instanse->rgb_buffer_size);
	if(p_instanse->yuv_buffer == NULL || p_instanse->rgb_buffer == NULL)
	{        
		fprintf(stderr ,"libvideoencoder: Error malloc..\n");        
		return NULL;    
	}
	
	p_instanse->x264_parameter=(x264_param_t *)malloc(sizeof(x264_param_t));
	if(!p_instanse->x264_parameter)
	{        
		fprintf(stderr ,"libvideoencoder: malloc x264_parameter error..\n");        
		return NULL;    
	}

	memset(p_instanse->x264_parameter,0,sizeof(p_instanse->x264_parameter));

	strcpy(p_instanse->parameter_preset,ENCODER_PRESET);
	strcpy(p_instanse->parameter_tune,ENCODER_TUNE);
	strcpy(p_instanse->parameter_profile,ENCODER_PROFILE);

	x264_param_default(p_instanse->x264_parameter);
	ret = x264_param_default_preset(p_instanse->x264_parameter,p_instanse->parameter_preset,p_instanse->parameter_tune);
	if(ret < 0)
	{
		fprintf(stderr ,"libvideoencoder: x264_param_default_preset error..\n");        
		return NULL; 
	}

	p_instanse->x264_parameter->i_fps_den 		 =1;
	p_instanse->x264_parameter->i_fps_num 		 =p_instanse->fps;
	p_instanse->x264_parameter->i_width 		 =p_instanse->width;
	p_instanse->x264_parameter->i_height		 =p_instanse->height;
	p_instanse->x264_parameter->i_threads		 =1;
	p_instanse->x264_parameter->i_keyint_max     =25;
	p_instanse->x264_parameter->i_keyint_min     =25;
	p_instanse->x264_parameter->i_scenecut_threshold     =0;
	p_instanse->x264_parameter->b_annexb		 =1;

	ret = x264_param_apply_profile(p_instanse->x264_parameter,p_instanse->parameter_profile);
	if(ret < 0)
	{
		fprintf(stderr ,"libvideoencoder: x264_param_apply_profile error..\n");        
		return NULL; 
	}

	p_instanse->x264_encoder = x264_encoder_open(p_instanse->x264_parameter);
	if(p_instanse->x264_encoder == NULL)
	{
		fprintf(stderr ,"libvideoencoder: x264_encoder_open error..\n");        
		return NULL; 
	}
	p_instanse->colorspace = ENCODER_COLORSPACE;

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

	if(input_video_sample_size != p_instanse->width*p_instanse->height*4)
	{		 
		fprintf(stderr ,"libvideoencoder: input_video_sample_size=%d..but bgra size is width*height*4=%d \n",input_video_sample_size,p_instanse->width*p_instanse->height*4);		
		return -2;	  
	}

	int ret = -1;
	int all_size = 0;
	x264_picture_t pic_in;//yuv420p_picture;
	x264_picture_t pic_out;
	x264_nal_t *nals = NULL;
	int nnal = 0;

	int yuv_buffer_size = p_instanse->yuv_buffer_size;
	char *yuv_buffer = (char *)p_instanse->yuv_buffer;
	
	ret = BGRA_to_YUV420SP(p_instanse->width,p_instanse->height,input_video_sample,input_video_sample_size,yuv_buffer,&yuv_buffer_size);
	if(ret < 0)
	{
		fprintf(stderr ,"libvideoencoder: BGRA_to_YUV420P error ret=%d..\n",ret);
		return -4;
	}

	x264_picture_init(&pic_in);
	x264_picture_init(&pic_out);
	//printf("+++++pic_in.i_csp=%d,pic_in.i_plane=%d,pic_in.i_stride[0]=%d,pic_in.i_stride[1]=%d,pic_in.i_stride[2]=%d,pic_in.i_stride[3]=%d++++++\n",pic_in.img.i_csp,pic_in.img.i_plane,pic_in.img.i_stride[0],pic_in.img.i_stride[1],pic_in.img.i_stride[2],pic_in.img.i_stride[3]);

	pic_in.img.i_csp=p_instanse->colorspace;
	pic_in.img.i_plane=2;
	pic_in.img.i_stride[0]=p_instanse->width;
	pic_in.img.i_stride[1]=p_instanse->width;
	pic_in.img.plane[0] = (uint8_t *)yuv_buffer;
	pic_in.img.plane[1] = (uint8_t *)yuv_buffer + p_instanse->width*p_instanse->height;

	pic_in.i_pts = p_instanse->i_pts++;
	ret = x264_encoder_encode(p_instanse->x264_encoder, &nals, &nnal, &pic_in, &pic_out);
	if(ret < 0)
	{
		fprintf(stderr ,"libvideoencoder: x264_encoder_encode error ret=%d..\n",ret);
		//x264_picture_clean(&pic_out);
		return -5;
	}
	
	//printf("+++++pic_out.i_csp=%d,pic_out.i_plane=%d,pic_out.i_stride[0]=%d,pic_out.i_stride[1]=%d,pic_out.i_stride[2]=%d,pic_out.i_stride[3]=%d++++++\n",pic_out.img.i_csp,pic_out.img.i_plane,pic_out.img.i_stride[0],pic_out.img.i_stride[1],pic_out.img.i_stride[2],pic_out.img.i_stride[3]);
	//printf("++++++pic_out.i_type=%d+++++nnal=%d++++++++nals->i_payload=%d+++++++++++\n",pic_out.i_type,nnal,nals->i_payload);

	all_size = ret;

	int padding = 2048 - all_size;
	if(padding < 0)
		padding = 0;

	p_instanse->real_count_fps++;
	
	if(*output_video_sample_size < all_size + padding)
	{		 
		fprintf(stderr ,"libvideosource: Error output_video_sample_size is too small..\n");
		//x264_picture_clean(&pic_out);
		return -6;	  
	}

	memcpy(output_video_sample,nals->p_payload,all_size);
	memset(output_video_sample + all_size,0,padding);
	*output_video_sample_size = all_size + padding;
	//x264_picture_clean(&pic_out);
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


	int ret = -1;
	int all_size = 0;
	x264_picture_t pic_in;//yuv420p_picture;
	x264_picture_t pic_out;
	x264_nal_t *nals = NULL;
	int nnal = 0;


	int width = p_instanse->width;
	int height = p_instanse->height;

	//printf("x=%d,y=%d,w=%d,h=%d\n",x,y,w,h);

	int rgb_buffer_size = p_instanse->rgb_buffer_size;
	unsigned char* rgb_buffer = p_instanse->rgb_buffer;
	if(rgb_buffer == NULL)
	{        
		fprintf(stderr ,"libvideoencoder: Error malloc ..\n");        
		return -2;    
	}
	int yuv_buffer_size = p_instanse->yuv_buffer_size;
	char *yuv_buffer = (char *)p_instanse->yuv_buffer;
	if(yuv_buffer == NULL)
	{		 
		fprintf(stderr ,"libvideoencoder: Error malloc ..\n");		  
		return -2;	  
	}

	//copy outbf->rgb_buffer
	
	for(int i = 0;i < h;i++)
		memcpy(rgb_buffer + width * (i+ y) * 4 + x * 4,input_video_sample + w *i * 4,w * 4);
	

	ret = BGRA_to_YUV420SP(p_instanse->width,p_instanse->height,(const char*)rgb_buffer,rgb_buffer_size,yuv_buffer,&yuv_buffer_size);
	if(ret < 0)
	{
		fprintf(stderr ,"libvideoencoder: BGRA_to_YUV420SP error ret=%d..\n",ret);
		return -4;
	}


	x264_picture_init(&pic_in);
	x264_picture_init(&pic_out);
	//printf("+++++pic_in.i_csp=%d,pic_in.i_plane=%d,pic_in.i_stride[0]=%d,pic_in.i_stride[1]=%d,pic_in.i_stride[2]=%d,pic_in.i_stride[3]=%d++++++\n",pic_in.img.i_csp,pic_in.img.i_plane,pic_in.img.i_stride[0],pic_in.img.i_stride[1],pic_in.img.i_stride[2],pic_in.img.i_stride[3]);

	pic_in.img.i_csp=p_instanse->colorspace;
	pic_in.img.i_plane=2;
	pic_in.img.i_stride[0]=p_instanse->width;
	pic_in.img.i_stride[1]=p_instanse->width;
	pic_in.img.plane[0] = (uint8_t *)yuv_buffer;
	pic_in.img.plane[1] = (uint8_t *)yuv_buffer + p_instanse->width*p_instanse->height;

	pic_in.i_pts = p_instanse->i_pts++;
	ret = x264_encoder_encode(p_instanse->x264_encoder, &nals, &nnal, &pic_in, &pic_out);
	if(ret < 0)
	{
		fprintf(stderr ,"libvideoencoder: x264_encoder_encode error ret=%d..\n",ret);
		//x264_picture_clean(&pic_out);
		return -5;
	}
	
	//printf("+++++pic_out.i_csp=%d,pic_out.i_plane=%d,pic_out.i_stride[0]=%d,pic_out.i_stride[1]=%d,pic_out.i_stride[2]=%d,pic_out.i_stride[3]=%d++++++\n",pic_out.img.i_csp,pic_out.img.i_plane,pic_out.img.i_stride[0],pic_out.img.i_stride[1],pic_out.img.i_stride[2],pic_out.img.i_stride[3]);
	//printf("++++++pic_out.i_type=%d+++++nnal=%d++++++++nals->i_payload=%d+++++++++++\n",pic_out.i_type,nnal,nals->i_payload);

	all_size = ret;
	
	int padding = 2048 - all_size;
	if(padding < 0)
		padding = 0;

	p_instanse->real_count_fps++;
	
	if(*output_video_sample_size < all_size + padding)
	{		 
		fprintf(stderr ,"libvideosource: Error output_video_sample_size is too small..\n");
		//x264_picture_clean(&pic_out);
		return -6;	  
	}

	memcpy(output_video_sample,nals->p_payload,all_size);
	memset(output_video_sample + all_size,0,padding);
	*output_video_sample_size = all_size + padding;
	//x264_picture_clean(&pic_out);
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
		fprintf(stderr ,"libaudioencoder: Error paraments..\n");        
		return -1;    
	}
	videoencoder_instanse *p_instanse = (videoencoder_instanse *)videoencoder;

	if(p_instanse->x264_encoder != NULL)
		x264_encoder_close(p_instanse->x264_encoder);
	if(p_instanse->x264_parameter!= NULL)
		free(p_instanse->x264_parameter);
	if(p_instanse->rgb_buffer != NULL)
		free(p_instanse->rgb_buffer);
	if(p_instanse->yuv_buffer != NULL)
		free(p_instanse->yuv_buffer);

	free(p_instanse);

	return 0;
}


