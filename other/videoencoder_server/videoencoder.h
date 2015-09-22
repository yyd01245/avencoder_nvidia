#ifndef __VIDEO_ENCODER_H__
#define __VIDEO_ENCODER_H__

typedef struct
{
	int width;
	int height;
	int gopsize_min;
	int gopsize_max;
	int bitrate;//bps
	int fps;
}InputParams_videoencoder;


typedef void video_encoder_t;

video_encoder_t* init_video_encoder(InputParams_videoencoder *pInputParams);
int encode_video_sample_inc(video_encoder_t *videoencoder,const char *input_video_sample,int input_video_sample_size,
	char *output_video_sample,int *output_video_sample_size,int x,int y,int w,int h);
int uninit_video_encoder(video_encoder_t *videoencoder);

#endif


