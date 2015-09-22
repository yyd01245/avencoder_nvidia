#include "audioencoder.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

int main()
{
	struct timeval tv;
	long now_time_ms;
	
	int ret = -1;
	/////////////////////////////////////////////////////////
	gettimeofday(&tv,NULL);
	now_time_ms = tv.tv_sec*1000 + tv.tv_usec/1000;
	printf("now_time_ms = %ld\n",now_time_ms);
	///////////////////////////////////////////////////////
	audio_encoder_t *encoder = init_audio_encoder();
	/////////////////////////////////////////////////////////
	gettimeofday(&tv,NULL);
	now_time_ms = tv.tv_sec*1000 + tv.tv_usec/1000;
	printf("now_time_ms = %ld\n",now_time_ms);
	///////////////////////////////////////////////////////
	if(encoder == NULL)
	{
		printf("init_audio_encoder fail\n");
		return -1;
	}

	char *input_audio_sample = (char *)malloc(1024);
	int input_audio_sample_size = 1024;
	char *output_audio_sample = (char *)malloc(10*1024);
	int output_audio_sample_size = 10*1024;

	for(int i = 0;i<100;i++)
	{

		ret = encode_audio_sample(encoder,input_audio_sample,input_audio_sample_size,output_audio_sample,&output_audio_sample_size);



		if(ret < 0)
		{
			printf("encode_audio_sample fail ret = %d\n",ret);
			return -2;
		}
		usleep(1000*100);
	}

	/////////////////////////////////////////////////////////
	gettimeofday(&tv,NULL);
	now_time_ms = tv.tv_sec*1000 + tv.tv_usec/1000;
	printf("now_time_ms = %ld\n",now_time_ms);
	///////////////////////////////////////////////////////
	ret = uninit_audio_encoder(encoder);
	/////////////////////////////////////////////////////////
	gettimeofday(&tv,NULL);
	now_time_ms = tv.tv_sec*1000 + tv.tv_usec/1000;
	printf("now_time_ms = %ld\n",now_time_ms);
	///////////////////////////////////////////////////////
	if(ret < 0)
	{
		printf("uninit_audio_encoder fail ret = %d\n",ret);
		return -3;
	}

	printf("uninit_audio_encoder success\n");
	return 0;

}

