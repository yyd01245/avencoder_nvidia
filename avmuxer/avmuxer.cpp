#include "avmuxer.h"
#include "CAVMuxer.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct
{
	CAVMuxer *muxer;
}avmuxer_instanse;


ts_muxer_t* init_ts_muxer(InputParams_avmuxer* pInputParams)
{
	if(pInputParams == NULL)
	{        
		fprintf(stderr ,"libavmuxer: Error paraments..\n");        
		return NULL;    
	}

	avmuxer_instanse *p_instanse = (avmuxer_instanse *)malloc(sizeof(avmuxer_instanse));
	if(p_instanse == NULL)
		return NULL;

	p_instanse->muxer = new CAVMuxer;

	InputParams *inputparam = (InputParams *)pInputParams;

	if(p_instanse->muxer->Init(inputparam) < 0)
	{        
		fprintf(stderr ,"libavmuxer: init muxer fail..\n");        
		return NULL;    
	}

	
	return p_instanse;
}

int write_video_frame(ts_muxer_t *avmuxer,const char *video_buffer,int video_buffer_size)
{
	if(avmuxer == NULL || video_buffer == NULL)
	{		 
		fprintf(stderr ,"libavmuxer: Error paraments..\n");		
		return -1;	  
	}
	avmuxer_instanse *p_instanse = (avmuxer_instanse *)avmuxer;
	int ret = p_instanse->muxer->WriteVideoFrame((char *)video_buffer, video_buffer_size);

	return ret;
}

int write_audio_frame(ts_muxer_t *avmuxer,const char *audio_buffer,int audio_buffer_size)
{
	if(avmuxer == NULL || audio_buffer == NULL)
	{		 
		fprintf(stderr ,"libavmuxer: Error paraments..\n"); 	
		return -1;	  
	}
	avmuxer_instanse *p_instanse = (avmuxer_instanse *)avmuxer;
	int ret = p_instanse->muxer->WriteAudioFrame((char *)audio_buffer, audio_buffer_size);

	return ret;
}


int video_pts(ts_muxer_t *avmuxer,double *videopts)
{
	if(avmuxer == NULL || videopts == NULL)
	{		 
		fprintf(stderr ,"libavmuxer: Error paraments..\n"); 	
		return -1;	  
	}
	avmuxer_instanse *p_instanse = (avmuxer_instanse *)avmuxer;
	*videopts = p_instanse->muxer->GetVideoPts();

	return 0;
}


int audio_pts(ts_muxer_t *avmuxer,double *audiopts)
{
	if(avmuxer == NULL || audiopts == NULL)
	{		 
		fprintf(stderr ,"libavmuxer: Error paraments..\n"); 	
		return -1;	  
	}
	avmuxer_instanse *p_instanse = (avmuxer_instanse *)avmuxer;
	*audiopts = p_instanse->muxer->GetAudioPts();

	return 0;
}

unsigned long get_bbcvudp_real_send_bytes(ts_muxer_t *avmuxer)
{
	if(avmuxer == NULL )
	{		 
		fprintf(stderr ,"libavmuxer: Error paraments..\n"); 	
		return -1;	  
	}
	avmuxer_instanse *p_instanse = (avmuxer_instanse *)avmuxer;
	
	
	return p_instanse->muxer->GetSendBytes();
}



int uninit_ts_muxer(ts_muxer_t *avmuxer)
{
	if(avmuxer == NULL)
	{		 
		fprintf(stderr ,"libavmuxer: Error paraments..\n");		
		return -1;	  
	}
	avmuxer_instanse *p_instanse = (avmuxer_instanse *)avmuxer;

	delete p_instanse->muxer;
	free(p_instanse);
	return 0;
}




