#ifndef __AUDIO_ENCODER_H__
#define __AUDIO_ENCODER_H__

typedef void audio_encoder_t;

audio_encoder_t* init_audio_encoder();
int encode_audio_sample(audio_encoder_t *audioencoder,const char *input_audio_sample,int input_audio_sample_size,
	char *output_audio_sample,int *output_audio_sample_size);
int uninit_audio_encoder(audio_encoder_t *audioencoder);

#endif


