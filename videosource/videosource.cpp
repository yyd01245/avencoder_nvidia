#include "videosource.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

typedef struct
{
	int flag;//0,unlock;1,lock
	int x;
	int y;
	int w;
	int h;
}shm_header;


typedef struct
{
	int width;
	int height;

	
	key_t key;
	int shm_id;
	void *shm_addr;
	unsigned int shm_size;
}videosource_instanse;

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
	
	p_instanse->key = (key_t)pInputParams->index;
	p_instanse->shm_size = sizeof(shm_header) + p_instanse->width*p_instanse->height*4;

	p_instanse->shm_id = shmget(p_instanse->key,p_instanse->shm_size, 0666|IPC_CREAT);
	if(p_instanse->shm_id == -1)
	{
		fprintf(stderr, "libvideosource shmget failed...\n");
		return NULL;
	}

	p_instanse->shm_addr = shmat(p_instanse->shm_id,NULL, 0);
	if(p_instanse->shm_addr == (void*)-1)
	{
		fprintf(stderr, "libvideosource shmat failed...\n");
		return NULL;
	}

	shm_header *p_shm_header = (shm_header *)p_instanse->shm_addr;
	p_shm_header->flag = 0;

	printf("libvideosource : index = %d width = %d height = %d key = 0x%x  shm_id = %d shm_addr = %p shm_size = %d\n",
		pInputParams->index,p_instanse->width,p_instanse->height,p_instanse->key,p_instanse->shm_id,p_instanse->shm_addr,p_instanse->shm_size);

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

	if(*video_sample_size < p_instanse->shm_size)
	{		 
		fprintf(stderr ,"libvideosource: Error video_sample_size is too small..\n");		
		return -2;	  
	}
	shm_header *p_shm_header = (shm_header *)p_instanse->shm_addr;
	char *video_data = (char *)p_instanse->shm_addr + sizeof(shm_header);
	
	//printf("libvideosource get_video_sample_all: p_shm_header->flag=%d.x=%d y=%d w=%d h=%d.\n",p_shm_header->flag,p_shm_header->x,p_shm_header->y,p_shm_header->w,p_shm_header->h);

	*x = 0;
	*y = 0;
	*w = p_instanse->width;
	*h = p_instanse->height;
	*video_sample_size = p_instanse->width*p_instanse->height*4;

	if(p_shm_header->flag == 1) 
		usleep(5*1000);
	
	if(p_shm_header->flag == 1)	
		printf("!!!!!!!libvideosource get_video_sample_all:p_shm_header->flag = %d.\n",p_shm_header->flag);
	p_shm_header->flag = 1;
	
	memcpy(video_sample,video_data,*video_sample_size);
	
	p_shm_header->flag = 0;
	
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

	if(*video_sample_size < p_instanse->shm_size)
	{		 
		fprintf(stderr ,"libvideosource: Error video_sample_size is too small..\n");		
		return -2;	  
	}
	shm_header *p_shm_header = (shm_header *)p_instanse->shm_addr;
	char *video_data = (char *)p_instanse->shm_addr + sizeof(shm_header);
	
	//printf("libvideosource get_video_sample_inc: p_shm_header->flag=%d.x=%d y=%d w=%d h=%d.\n",p_shm_header->flag,p_shm_header->x,p_shm_header->y,p_shm_header->w,p_shm_header->h);

	int tmp_x,tmp_y,tmp_w,tmp_h;

	tmp_x = p_shm_header->x;
	tmp_y = p_shm_header->y;
	tmp_w = p_shm_header->w;
	tmp_h = p_shm_header->h;

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

	if(p_shm_header->flag == 1) 
		usleep(5*1000);
	
	if(p_shm_header->flag == 1) 
			printf("!!!!!!!libvideosource get_video_sample_inc:p_shm_header->flag = %d.\n",p_shm_header->flag);
	p_shm_header->flag = 1;
		

	for(int i = 0;i < tmp_h;i++)
		memcpy(video_sample + tmp_w *i * 4,video_data + p_instanse->width * (i+ tmp_y) * 4 + tmp_x * 4,tmp_w * 4);

	p_shm_header->flag = 0;
	
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

	if(shmdt(p_instanse->shm_addr) == -1)
	{
		fprintf(stderr, "libvideosource shmdt failed...\n");	
		return -1;	  
	}

	free(p_instanse);

	return 0;
}


