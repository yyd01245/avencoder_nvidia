#ifndef __VIDEO_ENCODER_H__
#define __VIDEO_ENCODER_H__

#ifdef __cplusplus
extern "C" {
#endif

//#include "vaenclib.h"
//#include "Nvidia_EncodeSDK.h"

//#include <ipp.h>
//#include <ippcc.h>

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
    int devID;
}InputParams_videoencoder;


typedef void video_encoder_t;

video_encoder_t* init_video_encoder(InputParams_videoencoder *pInputParams);
int encode_video_sample(video_encoder_t *videoencoder,const char *input_video_sample,int input_video_sample_size,
	char *output_video_sample,int *output_video_sample_size);
int encode_video_sample_inc(video_encoder_t *videoencoder,const char *input_video_sample,int input_video_sample_size,
	char *output_video_sample,int *output_video_sample_size,int x,int y,int w,int h);
unsigned long get_real_count_fps(video_encoder_t *videoencoder);
int uninit_video_encoder(video_encoder_t *videoencoder);

#endif


