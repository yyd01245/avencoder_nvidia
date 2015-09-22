#include "videosource.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>

#include <pthread.h>

#include <rfb/rfbclient.h>

typedef struct
{
	int width;
	int height;

	rfbClient* client;

	int updateRect_x;
	int updateRect_y;
	int updateRect_w;
	int updateRect_h;
	char *frameBuffer;

	pthread_t tid;
	
}videosource_instanse;

static void update(rfbClient* client,int x,int y,int w,int h) 
{
	rfbClientLog("1 Received an update for %d,%d,%d,%d.\n",x,y,w,h);
	//rfbClientLog("2 Received an update for %d,%d,%d,%d.\n",client->updateRect.x,client->updateRect.y,client->updateRect.w,client->updateRect.h);
}

void* HandleServerMessage_Process(void *param)
{
	videosource_instanse *p_instanse = (videosource_instanse *)param;
	
	while (1) 
	{		
		static int i=0;		
		fprintf(stderr,"\r%d",i++);		
		if(WaitForMessage(p_instanse->client,50)<0)			
			break;		
		if(!HandleRFBServerMessage(p_instanse->client))			
			break;	
	}
}

video_source_t* init_video_source(InputParams_videosource *pInputParams)
{
	if(pInputParams == NULL)
	{        
		fprintf(stderr ,"libvideosource: Error paraments..\n");        
		return NULL;    
	}

	
	videosource_instanse *p_instanse = (videosource_instanse *)malloc(sizeof(videosource_instanse));
	if(p_instanse == NULL)
	{
		fprintf(stderr, "libvideosource malloc videosource_instanse failed...\n");
		return NULL;
	}

	p_instanse->width = pInputParams->width;
	p_instanse->height = pInputParams->height;

	p_instanse->client = rfbGetClient(8,3,4);
	p_instanse->client->GotFrameBufferUpdate=update;
/*
	p_instanse->frameBuffer = (char *)malloc(p_instanse->width *p_instanse->height *4);
	if(p_instanse->frameBuffer == NULL)
	{
		fprintf(stderr, "libvideosource malloc p_instanse->frameBuffer failed...\n");
		return NULL;
	}
	p_instanse->client->frameBuffer = (uint8_t*)p_instanse->frameBuffer;
	*/
	p_instanse->client->appData.encodingsString="raw";

	int argc = 2;
	char * parg[2];

	parg[0] = "vncclient";
	parg[1] = "127.0.0.1:5902";

	if (!rfbInitClient(p_instanse->client,&argc,parg))		
		return NULL;

	p_instanse->frameBuffer = (char *)p_instanse->client->frameBuffer;

	int ret = pthread_create(&p_instanse->tid, NULL, HandleServerMessage_Process, p_instanse);
	if(ret != 0)
	{
		printf("libaudiosource: pthread_create fail!...\n");
		return NULL;
	}
	
	
	return p_instanse;	
}

int get_video_sample_all(video_source_t *source,char *video_sample,int *video_sample_size,int *x,int *y,int *w,int *h)
{
	if(source == NULL || video_sample == NULL || video_sample_size == NULL || x == NULL || y == NULL || w == NULL || h == NULL)
	{        
		fprintf(stderr ,"libvideosource: Error paraments..\n");        
		return -1;    
	}
	videosource_instanse *p_instanse = (videosource_instanse *)source;

	*x = 0;
	*y = 0;
	*w = 1280;
	*h = 720;
	*video_sample_size = 1280 * 720 * 4;

	memcpy(video_sample,p_instanse->frameBuffer,*video_sample_size);
	
	return 0;
}

int get_video_sample_inc(video_source_t *source,char *video_sample,int *video_sample_size,int *x,int *y,int *w,int *h)
{
	if(source == NULL || video_sample == NULL || video_sample_size == NULL || x == NULL || y == NULL || w == NULL || h == NULL)
	{        
		fprintf(stderr ,"libvideosource: Error paraments..\n");        
		return -1;    
	}
	videosource_instanse *p_instanse = (videosource_instanse *)source;

	*x = 0;
	*y = 0;
	*w = 1280;
	*h = 720;
	*video_sample_size = 1280 * 720 * 4;

	memcpy(video_sample,p_instanse->frameBuffer,*video_sample_size);
	

/*
	int tmp_x,tmp_y,tmp_w,tmp_h;

	tmp_x = p_instanse->updateRect_x;
	tmp_y = p_instanse->updateRect_y;
	tmp_w = p_instanse->updateRect_w;
	tmp_h = p_instanse->updateRect_h;

	if(tmp_w == 0 || tmp_h == 0)
	{
		tmp_x = 0;
		tmp_y = 0;
	}

	if(tmp_x %2 != 0)
	{
		tmp_x--;
		tmp_w++;
	}
	if(tmp_y %2 != 0)
	{
		tmp_y--;
		tmp_h++;
	}
	if(tmp_w %2 != 0)
	{
		tmp_w++;
	}
	if(tmp_h %2 != 0)
	{
		tmp_h++;
	}
	
	*x = tmp_x;
	*y = tmp_y;
	*w = tmp_w;
	*h = tmp_h;
	*video_sample_size = tmp_w * tmp_h * 4;

	
	for(int i = 0;i < tmp_h;i++)
		memcpy(video_sample + tmp_w *i * 4,p_instanse->frameBuffer + p_instanse->width * (i+ tmp_y) * 4 + tmp_x * 4,tmp_w * 4);
	*/
	return 0;
}

int uninit_video_source(video_source_t *source)
{
	if(source == NULL)
	{		 
		fprintf(stderr ,"libvideosource: Error paraments..\n");		
		return -1;	  
	}
	videosource_instanse *p_instanse = (videosource_instanse *)source;


	free(p_instanse);

	return 0;
}


