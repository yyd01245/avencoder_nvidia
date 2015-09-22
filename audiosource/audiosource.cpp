#include "audiosource.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h> 
#include <sys/stat.h> 

#include <queue>
#include <pthread.h>

using namespace std;

typedef short int int16_t; 

typedef struct
{		
	int16_t *m_AudioBuff;		
	int m_samplesSize;	
}m_Audio;


typedef struct
{
	int index;
	int m_samplesSize;
	int fd;
	bool flag_stop;
	pthread_mutex_t mutex;
	queue<m_Audio> *m_AudioQueue;
	int queue_max_size;
}audiosource_instanse;


void* StartGetAudio(void *param)
{
	audiosource_instanse *p_instanse = (audiosource_instanse *)param;
	
	int ret;
	m_Audio sample;	
	
	sample.m_samplesSize = p_instanse->m_samplesSize;	
	//printf("sample.m_samplesSize %d \n",audiosample->m_samplesSize);	
	
	
	while(!p_instanse->flag_stop)
	{
		sample.m_AudioBuff = (int16_t*)calloc(sample.m_samplesSize,sizeof(int16_t));
		if(sample.m_AudioBuff == NULL)
		{
			printf("error:calloc fail!");
			break;
		}
		
		if((read(p_instanse->fd,(unsigned char*)sample.m_AudioBuff,sample.m_samplesSize)) < 0)      	
		{
			perror("read");
			break;
		}

		pthread_mutex_lock(&p_instanse->mutex);
		p_instanse->m_AudioQueue->push(sample);
		if(p_instanse->m_AudioQueue->size() > p_instanse->queue_max_size)		
		{
			free(p_instanse->m_AudioQueue->front().m_AudioBuff);
			p_instanse->m_AudioQueue->front().m_AudioBuff = NULL;
			p_instanse->m_AudioQueue->pop();
		}
		pthread_mutex_unlock(&p_instanse->mutex);
	}
}


audio_source_t* init_audio_source(InputParams_audiosource *pInputParams)
{
	if(pInputParams == NULL)
	{        
		fprintf(stderr ,"libaudiosource: Error paraments..\n");        
		return NULL;    
	}

	audiosource_instanse *p_instanse = (audiosource_instanse *)malloc(sizeof(audiosource_instanse));
	if(p_instanse == NULL)
	{        
		fprintf(stderr ,"libaudiosource: malloc audiosource_instanse fail..\n");        
		return NULL;    
	}
	queue<m_Audio> *m_AudioQueue = new queue<m_Audio>;
	if(m_AudioQueue == NULL)
	{
		printf("libaudiosource:new m_AudioQueue fail..!\n");
		return NULL;
	}

	p_instanse->index = pInputParams->index;
	p_instanse->m_samplesSize = 4608;
	p_instanse->flag_stop = false;
	p_instanse->queue_max_size = 8;
	p_instanse->m_AudioQueue = m_AudioQueue;

	char pipename[32] = {0};    	
	sprintf(pipename, "/tmp/pulse-00%d", p_instanse->index);        
	//print("pipename is  %s \n",pipename);
	if((mkfifo(pipename, O_CREAT|O_RDWR|0777)) && (errno != EEXIST))  
	{
		fprintf(stderr,"libaudiosource : mkfifo pipe failed: errno=%d\n",errno);
		return NULL;
	}
	
	chmod(pipename,0777);
	
	if((p_instanse->fd = open(pipename, O_RDWR)) < 0)          
	{          	
		perror("libaudiosource: open");          	
		return NULL;      	
	}
	
	pthread_mutex_init(&p_instanse->mutex,NULL);

	pthread_t pthread;
	int ret = pthread_create(&pthread, NULL, StartGetAudio, p_instanse);
	if(ret != 0)
	{
		printf("libaudiosource: pthread_create fail!...\n");
		return NULL;
	}

	return p_instanse;
}

int get_audio_sample(audio_source_t *source,char *audio_sample,int *audio_sample_size)
{
	m_Audio sample;
	int ret = -1;
	if(source == NULL || audio_sample == NULL || audio_sample_size == NULL)
	{        
		fprintf(stderr ,"libaudiosource: Error paraments..\n");        
		return -1;    
	}

	audiosource_instanse *p_instanse = (audiosource_instanse *)source;

	if(p_instanse->m_AudioQueue->size() <= 0)
	{
		*audio_sample_size = 0;
		return 0;
	}

	pthread_mutex_lock(&p_instanse->mutex);

	sample.m_AudioBuff = p_instanse->m_AudioQueue->front().m_AudioBuff;
	sample.m_samplesSize = p_instanse->m_AudioQueue->front().m_samplesSize;

	p_instanse->m_AudioQueue->pop();

	pthread_mutex_unlock(&p_instanse->mutex);


	if(*audio_sample_size < sample.m_samplesSize)
	{        
		fprintf(stderr ,"libaudiosource: audio_sample_size is toot small..\n");
		free(sample.m_AudioBuff);
		return -2;    
	}

	memcpy(audio_sample,sample.m_AudioBuff,sample.m_samplesSize);
	*audio_sample_size = sample.m_samplesSize;

	free(sample.m_AudioBuff);
		
	return 1;
}

int uninit_audio_source(audio_source_t *source)
{
	if(source == NULL)
	{		 
		fprintf(stderr ,"libaudiosource: Error paraments..\n");		
		return -1;	  
	}
	audiosource_instanse *p_instanse = (audiosource_instanse *)source;
	p_instanse->flag_stop = true;

	
	while(p_instanse->m_AudioQueue->size() > 0)
	{
		free(p_instanse->m_AudioQueue->front().m_AudioBuff);
		p_instanse->m_AudioQueue->front().m_AudioBuff = NULL;
		p_instanse->m_AudioQueue->pop();
	}

	pthread_mutex_destroy(&p_instanse->mutex);

	if(p_instanse->m_AudioQueue != NULL)
		delete p_instanse->m_AudioQueue;
		
	free(p_instanse);
	
	return 0;
}


