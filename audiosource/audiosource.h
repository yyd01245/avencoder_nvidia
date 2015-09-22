#ifndef __AUDIO_SOURCE_H__
#define __AUDIO_SOURCE_H__

typedef void audio_source_t;

typedef struct
{
	int index;	
}InputParams_audiosource;

audio_source_t* init_audio_source(InputParams_audiosource *pInputParams);
int get_audio_sample(audio_source_t *source,char *audio_sample,int *audio_sample_size);
int uninit_audio_source(audio_source_t *source);

#endif


