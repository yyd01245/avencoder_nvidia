#ifndef __VEDIO_SOURCE_H__
#define __VEDIO_SOURCE_H__

typedef void video_source_t;

typedef struct
{
	int index;
	int width;
	int height;
}InputParams_videosource;

video_source_t* init_video_source(InputParams_videosource *pInputParams);
int get_video_sample_all(video_source_t *source,char *video_sample,int *video_sample_size,int *x,int *y,int *w,int *h);
int get_video_sample_inc(video_source_t *source,char *video_sample,int *video_sample_size,int *x,int *y,int *w,int *h);
int uninit_video_source(video_source_t *source);

#endif


