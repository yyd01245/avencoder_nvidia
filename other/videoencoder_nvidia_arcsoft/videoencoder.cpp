#include "videoencoder.h"
#include <sys/time.h>

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>
#include <dlfcn.h>
#include <pthread.h>

#include <fcntl.h>
#include <sys/types.h>

//#include "utils.h"
#include "IASVDecoder.h"
#include "ASVEncoder_interface.h"
#include "ippcc.h"


extern int get_gpu_id_mem();
extern int get_gpu_id_proc();
extern int get_id(int);

struct coded_buff
{
	unsigned char *buff;
	unsigned int length;
};


typedef struct tag_ASVEncoderAPI
{
	void *handle;
	pASVECreate pCreate;
	pASVEEncode pEncoder;
	pASVEEndOfStream pEndOfStream;
	pASVEDelete pDelete;
} ASVEncoderAPI;


typedef struct tag_SimpleTranscoder
{
	void *hDecoder;
	ASVEHANDLE hEncoder;
	sem_t mutex;
	pthread_mutex_t encode_mutex;
	char inFile[255];
	char outFile[255];
	Int64 llFrames;
	ASVEncoderAPI EncAPI;
	ASVEncoderOutSample *pOutSample;
	ASVEncoderInSample *pInSample;
	bool hasFlushCodeData;
} SimpleTranscoder;

typedef struct
{
	int width;
	int height;
	int gopsize_min;
	int gopsize_max;
	int bitrate;//bps
	int fps;

    int devID;

    ASVEncoderRect*rect;//=(ASVEncoderRect*)calloc(1,sizeof(ASVEncoderRect));
	unsigned char *outbf;//yuv data
	unsigned char *enc_buf;//es data
	unsigned char *rgb_buffer;//for incremental encoder
	SimpleTranscoder *pSTranscoder;

	//for statistics
	unsigned long real_count_fps;
		unsigned char *buff_tmp;
}videoencoder_instanse;

static void GetDefaultEncoderParameter(ASVEncoderParam *pParam)
{
	memset(pParam, 0, sizeof(ASVEncoderParam));
	
	//below should be set by app
//	pParam->width;
//	pParam->height;
//	pParam->aspect_ratio_y; 
//	pParam->aspect_ratio_x;
//	pParam->frame_rate;

	//pParam->video_format_type = 2;
	pParam->profile = H264_MAIN_PROFILE;
	pParam->level = 41;

	pParam->idr_interval = 30;
	pParam->successive_bframes = 0;
	pParam->num_slices = 1;
	pParam->thread_count = 1;
	pParam->closed_gop = 1;
	pParam->enable_adaptive_gop = 0;

	pParam->entropy_method = 1;//!CABAC
	pParam->enable_deblock = 1; 
	pParam->maxnum_ref_forward = 1;

	pParam->interlace = 0; //0:frame; 1: mbaff; 2: field	
	pParam->top_field_first = -1;

	pParam->video_quality_level = 2; //2

	pParam->rate_control_type = 1;//CUDA:0-FQ, 1-CBR, 2-VBR
	pParam->qp_i = pParam->qp_p = pParam->qp_b = 20;

	pParam->target_bit_rate = 3000;
	pParam->max_bit_rate = pParam->target_bit_rate * 1.5;
	pParam->bit_rate_buffer_size = pParam->max_bit_rate;
	   
	pParam->delay = 0.5;   		

	pParam->hDevice = 0;
	
	pParam->use_aud_unit = 1;
	
	//below ignored by now
//	pParam->video_format_type;
//	pParam->stride;
//	pParam->vstride;
//	pParam->pContext;
//	pParam->pMutex;
//	pParam->bDecoderContext;
}

bool LoadAPI(ASVEncoderAPI *pEnc)
{

	//void *hEncoder = dlopen(ASVE_ENCODER_DLL, RTLD_LAZY);
	void *hEncoder = dlopen("/usr/local/lib/ASVEncoder.so", RTLD_LAZY);
	if (hEncoder == NULL)
	{
		return false;
	}
	
	pEnc->handle = hEncoder;
	pEnc->pCreate = (pASVECreate)dlsym(hEncoder, "ASVE_Create");
	pEnc->pEncoder = (pASVEEncode)dlsym(hEncoder, "ASVE_Encode");
	pEnc->pEndOfStream = (pASVEEndOfStream)dlsym(hEncoder, "ASVE_EndOfStream");
	pEnc->pDelete = (pASVEDelete)dlsym(hEncoder, "ASVE_Delete");
	
	return true;
}

FILE *fpfile = NULL;
Int32 EncoderDeliveryNotify(void *caller, void *pUserData, ASVEncoderOutSample *pSample)
{
	SimpleTranscoder *pSTrans = (SimpleTranscoder *)caller;
	//ASVEncoderOutSample *pOutSample = (ASVEncoderOutSample *)pSample;
	
	if (pSTrans == NULL || pSample == NULL)
		return -1;
	
	if (pSample->lBufSize > 0)
	{

		pthread_mutex_lock(&pSTrans->encode_mutex);
		memcpy(pSTrans->pOutSample->pbBuffer,pSample->pbBuffer,pSample->lBufSize);
		pSTrans->pOutSample->lBufSize = pSample->lBufSize;
		pSTrans->pOutSample->lFrameType = pSample->lFrameType;
		pSTrans->pOutSample->llStart= pSample->llStart;
		pSTrans->pOutSample->llEnd= pSample->llEnd;

		
		pSTrans->hasFlushCodeData = 1;
		pthread_mutex_unlock(&pSTrans->encode_mutex);
	}
	
	return 0;
} 




void* init_encoder(int argc, char *argv[])
{
	int width = atoi(argv[1]);
	int height = atoi(argv[2]);
	int bitrate;// =  atoi(argv[3]);
	int fps = atoi(argv[4]);
	int iGopSize = atoi(argv[5]);
    int devID;// = atoi(argv[6]);


    sscanf(argv[3],"fb=%d",&bitrate);
    sscanf(argv[4],"framerate=%d",&fps);
    sscanf(argv[5],"gopsize=%d",&iGopSize);
    sscanf(argv[6],"devid=%d",&devID);

	int codec_id;
	int ret = 0;

	printf("init encoder param w=%d,h=%d,bitrate=%d,fps=%d,gopsize=%d,devID=%d \n",width,
		height,bitrate,fps,iGopSize,devID);
	ASVEncoderParam EncoderParam;
	memset(&EncoderParam, 0, sizeof(ASVEncoderParam));

	ASVEncoderInSample InSample ={0};

	GetDefaultEncoderParameter(&EncoderParam);


    if(devID==-1){
        fprintf(stderr,"No devID specified..Auto Choose..\n");
        devID=get_gpu_id_mem();

    }
//    int id=get_id(8);
//    int id=get_next_gpuid();
//set id
    EncoderParam.hDevice=devID;

//parseINI


//
	EncoderParam.width = width;
	EncoderParam.height = height;
	EncoderParam.aspect_ratio_x = 16;
	EncoderParam.aspect_ratio_y = 9;
	EncoderParam.frame_rate = 25;
	EncoderParam.idr_interval = iGopSize ;//gopsize
//	EncoderParam.idr_interval = 25 ;//gopsize
	EncoderParam.target_bit_rate = 3000;
	EncoderParam.max_bit_rate = EncoderParam.target_bit_rate * 1.5;
	EncoderParam.rate_control_type = 1; //value for rate_control_type: CUDA:0-FQ; 1-CBR; 2-VBR;
	EncoderParam.bit_rate_buffer_size = 6000;
	//EncoderParam.max_bit_rate = 3500;
	

	SimpleTranscoder *pSTranscoder = new SimpleTranscoder;
	memset(pSTranscoder,0,sizeof(SimpleTranscoder));
	//SimpleTranscoder STranscoder;
	//memset(&STranscoder, 0, sizeof(SimpleTranscoder));

	EncoderParam.callback.caller = pSTranscoder;
	EncoderParam.callback.pInterface = EncoderDeliveryNotify;

	pSTranscoder->pOutSample = new ASVEncoderOutSample;
	memset(pSTranscoder->pOutSample,0,sizeof(ASVEncoderOutSample));
	unsigned char *buff = new unsigned char[5*1024*1024];
	pSTranscoder->pOutSample->pbBuffer = buff;

	pSTranscoder->pOutSample->llStart= 0;
	pSTranscoder->pOutSample->llEnd = 0;
	pSTranscoder->pOutSample->lBufSize = 0;
	pSTranscoder->pOutSample->lFrameType = 0;

	pSTranscoder->pInSample = new ASVEncoderInSample;
	memset(pSTranscoder->pInSample,0,sizeof(ASVEncoderInSample));
	pSTranscoder->pInSample->llStart = 0;
	pSTranscoder->pInSample->llEnd = 0;
	pSTranscoder->pInSample->width = width;
	pSTranscoder->pInSample->height = height;
	pSTranscoder->pInSample->stride = width;
	pSTranscoder->pInSample->vstride = height;
	pSTranscoder->pInSample->ulBufMode = 0;//now only support 0
	pSTranscoder->pInSample->ulColorSpace = 3; //0:NV12;1:YV12;2:I420;3:XRGB
    pSTranscoder->pInSample->pRect=NULL;

//    //for test
//        pSTranscoder->hasFlushCodeData=1;

	printf("-----video =%d %d \n",pSTranscoder->pInSample->width,pSTranscoder->pInSample->height);

	if (!LoadAPI(&pSTranscoder->EncAPI))
	{
		printf("library not exist.\n");
		fflush(stdout);
		goto handle_end;
	}

	sem_init(&pSTranscoder->mutex, 0, 0);
	pthread_mutex_init(&pSTranscoder->encode_mutex,NULL);

	printf("------------encode codetype = %d \n",EncoderParam.video_format_type);
	ret = pSTranscoder->EncAPI.pCreate(&EncoderParam, &pSTranscoder->hEncoder);
	if (ret < 0)
	{
/*		switch(ret)
		{
			case PARAMETER_INVALID:
				printf("The input parameter is invalid.\n");
				break;
*		PARAMETER_NOT_SUPPORT:	Only CAVLC is supported temporarily, if pPar->bCAVLC is set equal 0, 
*								PARAMETER_NOT_SUPPORT will be return.
*		MEMORY_ERROR:			Allcate memory for encoder handle failed
		}
*/		
        //for tst

		fprintf(stderr,"Create encoder failed. error num =%d \n",ret);

		goto handle_end;
	}

	return pSTranscoder;
	
handle_end:
		if (pSTranscoder->hEncoder)
			pSTranscoder->EncAPI.pDelete(pSTranscoder->hEncoder);
		if (pSTranscoder->EncAPI.handle)
			dlclose(pSTranscoder->EncAPI.handle);
		sem_destroy(&pSTranscoder->mutex);

		return NULL;


}

int encode_frame(SimpleTranscoder *pSTranscoder,unsigned char *inputdata)
{
		ASVEncoderInSample *pInSample = pSTranscoder->pInSample;

		pInSample->pbYUV[0] = inputdata;


		int ret = 0;
////for test
		ret = pSTranscoder->EncAPI.pEncoder(pSTranscoder->hEncoder, pInSample);
//        fprintf(stderr,"Call pEncoder.....\n");
		//get code data
		return ret;

}
int close_encoder(SimpleTranscoder *pSTranscoder)
{
	if(NULL == pSTranscoder)
	{
		return -1;
	}
	if (pSTranscoder->hEncoder)
		pSTranscoder->EncAPI.pDelete(pSTranscoder->hEncoder);
	if (pSTranscoder->EncAPI.handle)
		dlclose(pSTranscoder->EncAPI.handle);

	if(pSTranscoder->pOutSample)
	{
		if(pSTranscoder->pOutSample->pbBuffer)
			delete pSTranscoder->pOutSample->pbBuffer;
		pSTranscoder->pOutSample->pbBuffer = NULL;
		delete pSTranscoder->pOutSample;
		pSTranscoder->pInSample = NULL;
	}
	if(pSTranscoder->pInSample)
	{
		delete pSTranscoder->pInSample;
		pSTranscoder->pInSample = NULL;
	}
	sem_destroy(&pSTranscoder->mutex);
	pthread_mutex_destroy(&pSTranscoder->encode_mutex);
	delete pSTranscoder;
	pSTranscoder = NULL;

}


int GetCode_Data(SimpleTranscoder *pSTranscoder,struct coded_buff *avc_p)
{
	if(pSTranscoder == NULL || avc_p == NULL /*|| pSTranscoder->pOutSample == NULL || 
		NULL == pSTranscoder->pOutSample->pbBuffer || avc_p->buff==NULL*/)
	{
		printf("----Getcode Data error <arguments> %d \n",-1);
		return -1;
	}
	
	if(!pSTranscoder->hasFlushCodeData)
	{
		printf("----Getcode Data error <hasFlushCodeData> %d \n",-1);
		return -2;
	}
//	printf("---get data 1\n");
	pthread_mutex_lock(&pSTranscoder->encode_mutex);
	memcpy(avc_p->buff,pSTranscoder->pOutSample->pbBuffer,pSTranscoder->pOutSample->lBufSize);
	avc_p->length = pSTranscoder->pOutSample->lBufSize;


////for test
	pSTranscoder->hasFlushCodeData = 0;
	pthread_mutex_unlock(&pSTranscoder->encode_mutex);
	//printf("----get code data success len=%d\n",avc_p->length);
	return 1;
}

int HasCode_DataFlush(SimpleTranscoder *pSTranscoder)
{
	
	return pSTranscoder->hasFlushCodeData;
}

video_encoder_t* init_video_encoder(InputParams_videoencoder *pInputParams)
{
	if(pInputParams == NULL)
	{        
		fprintf(stderr ,"libvideoencoder: Error paraments..\n");        
		return NULL;    
	}

	
	videoencoder_instanse *p_instanse = (videoencoder_instanse *)malloc(sizeof(videoencoder_instanse));
	if(p_instanse == NULL)
	{        
		fprintf(stderr ,"libvideoencoder: Error malloc <%d>..\n",__LINE__);        
		return NULL;    
	}

	p_instanse->width = pInputParams->width;
	p_instanse->height = pInputParams->height;
	p_instanse->gopsize_min = pInputParams->gopsize_min;
	p_instanse->gopsize_max = pInputParams->gopsize_max;
	p_instanse->fps = pInputParams->fps;
	p_instanse->bitrate = pInputParams->bitrate/1024;
	p_instanse->real_count_fps = 0;
	
	p_instanse->devID = pInputParams->devID;

	p_instanse->buff_tmp = (unsigned char*)malloc(5*1024*1024);
	printf("----encode param w=%d,h=%d,gopsize=%d:%d,fps=%d,bitrate=%d,devID=%d\n",
		p_instanse->width,p_instanse->height,p_instanse->gopsize_min,p_instanse->gopsize_max,p_instanse->fps,
		p_instanse->bitrate,p_instanse->devID);

	unsigned char parn=0;
	char mkarg[30][30];
	char * parg[30];
	sprintf(mkarg[parn],"main");
	parg[parn]=mkarg[parn];
	parn++;
	sprintf(mkarg[parn],"%d",p_instanse->width);
	parg[parn]=mkarg[parn];
	parn++;
	sprintf(mkarg[parn],"%d",p_instanse->height);
	parg[parn]=mkarg[parn];
	parn++;

	if(p_instanse->width<=720)
		p_instanse->bitrate=4000;
	else if(p_instanse->width<=1280 && p_instanse->width>720 )
		p_instanse->bitrate=4000;
	else if(p_instanse->width>1280)
		p_instanse->bitrate=9000;

	sprintf(mkarg[parn],"fb=%d",p_instanse->bitrate);
	parg[parn]=mkarg[parn];
	parn++;
//	sprintf(mkarg[parn],"mode=%d\0",1);
//	parg[parn]=mkarg[parn];
//	parn++;
	sprintf(mkarg[parn],"framerate=%d",p_instanse->fps);
	parg[parn]=mkarg[parn];
	parn++;
//	sprintf(mkarg[parn],"gopsize=%d",p_instanse->gopsize_max);
	sprintf(mkarg[parn],"gopsize=%d",p_instanse->gopsize_min);
	parg[parn]=mkarg[parn];
	parn++;

    sprintf(mkarg[parn],"devid=%d",p_instanse->devID);
	parg[parn]=mkarg[parn];
	parn++;


	p_instanse->pSTranscoder = NULL;
	p_instanse->pSTranscoder = (SimpleTranscoder*)init_encoder(parn,parg);
	if(NULL == p_instanse->pSTranscoder)
	{
		fprintf(stderr ,"libvideoencoder: Init Nvidia encoder error..\n");
		return NULL;
	}
    p_instanse->rect=(ASVEncoderRect*)calloc(1,sizeof(ASVEncoderRect));
	p_instanse->outbf = (unsigned char *)malloc(p_instanse->width*p_instanse->height*2);
//	p_instanse->enc_buf = (unsigned char *)malloc(p_instanse->width*p_instanse->height*2);
//	p_instanse->rgb_buffer = (unsigned char *)malloc(p_instanse->width*p_instanse->height*4);
	if(p_instanse->outbf == NULL /*|| p_instanse->enc_buf == NULL || p_instanse->rgb_buffer == NULL*/)
	{        
		fprintf(stderr ,"libvideoencoder: Error malloc <%d>..\n",__LINE__);        
		return NULL;    
	}
	
	printf("----init encode success \n");
	return p_instanse;
}



int encode_video_sample(video_encoder_t *videoencoder,const char *input_video_sample,int input_video_sample_size,
	char *output_video_sample,int *output_video_sample_size)
{
	if(videoencoder == NULL || input_video_sample == NULL || output_video_sample == NULL || output_video_sample_size == NULL)
	{        
		fprintf(stderr ,"libvideoencoder: Error paraments..\n");        
		return -1;    
	}
	videoencoder_instanse *p_instanse = (videoencoder_instanse *)videoencoder;

	int width = p_instanse->width;
	int height = p_instanse->height;
	
	const char* outbf = input_video_sample;
	if(outbf == NULL)
	{        
		fprintf(stderr ,"libvideoencoder: Error malloc <%d> ..\n",__LINE__);        
		return -2;    
	}
	

///START::RGB2YUV    
/*
	int pDstStep[3];
	unsigned char *pDst[3];
	pDstStep[0]=width;
	pDstStep[1]=width/2;
	pDstStep[2]=width/2;

	IppiSize roiSize;
	roiSize.width=width;
	roiSize.height=height;

	pDst[0]=outbf;
	pDst[1]=outbf+width*height;
	pDst[2]=outbf+width*height+width*height/4;
	ippiBGRToYCbCr420_709CSC_8u_AC4P3R((unsigned char*)input_video_sample,width*4,pDst,pDstStep,roiSize);
*/
///END::RGB2YUV

	int enc_size,offset;


    ASVEncoderInSample* in_sample=p_instanse->pSTranscoder->pInSample;
    in_sample->pRect=NULL; 

	encode_frame(p_instanse->pSTranscoder,(unsigned char*)outbf);
	//printf("-----encode frame fun over \n");

	struct timeval tm;
	struct timeval tm1;
	gettimeofday(&tm,NULL);
	int iloop = 3;
	while(!HasCode_DataFlush(p_instanse->pSTranscoder))
	{
		usleep(5);
		if(iloop-- < 0)
			break;
	}

	if(iloop < 0)
	{
		gettimeofday(&tm1,NULL);
		long start_time_us,end_time_us;
		start_time_us  = tm.tv_sec*1000*1000 + tm.tv_usec;
		end_time_us = tm1.tv_sec*1000*1000 + tm1.tv_usec;
		printf("libvideoencoder:WARN:: Encoding elapsed %ld(us)\n",end_time_us-start_time_us);
	}
	struct coded_buff cdbf={0};
	
	cdbf.buff=p_instanse->buff_tmp;
	
	if(GetCode_Data(p_instanse->pSTranscoder,&cdbf)< 0)
	{
		fprintf(stderr ,"libvideoencoder:WARN:: get code data failed..\n");		
		return -3;	 
	}

    //	printf("---get code data success len=%d \n",cdbf.length);
	int padding = 512 - cdbf.length;
	if(padding < 0)
		padding = 0;

	p_instanse->real_count_fps++;

	if(*output_video_sample_size < cdbf.length + padding)
	{		 
		fprintf(stderr ,"libvideoencoder:Error:: output_video_sample_size is too small..\n");		
		return -3;	  
	} 
	
	memcpy(output_video_sample,cdbf.buff,cdbf.length);
	memset(output_video_sample + cdbf.length,0,padding);
	*output_video_sample_size = cdbf.length;// + padding

//    fprintf(stderr,"End encode full\n");

	return 0;
}

int encode_video_sample_inc(video_encoder_t *videoencoder,const char *input_video_sample,int input_video_sample_size,
	char *output_video_sample,int *output_video_sample_size,int x,int y,int w,int h)
{
	if(videoencoder == NULL || input_video_sample == NULL || output_video_sample == NULL || output_video_sample_size == NULL)
	{        
		fprintf(stderr ,"libvideoencoder: Error paraments..\n");        
		return -1;    
	}
//    fprintf(stderr,"INSIZ:%d \n x:%d, y:%d, w:%d, h:%d\n",input_video_sample_size,x,y,w,h);

	videoencoder_instanse *p_instanse = (videoencoder_instanse *)videoencoder;

	int width = p_instanse->width;
	int height = p_instanse->height;

	//printf("x=%d,y=%d,w=%d,h=%d\n",x,y,w,h);
    //
    ASVEncoderRect*rect=p_instanse->rect;//(ASVEncoderRect*)calloc(1,sizeof(ASVEncoderRect));

//    fprintf(stderr," Encode(%p) __%d__\n",rect,__LINE__);
    rect->left=x;
    rect->top=y;
    rect->width=w;
    rect->height=h;
    
    p_instanse->pSTranscoder->pInSample->pRect=rect;

	unsigned char* outbf = (unsigned char*)input_video_sample;

	encode_frame(p_instanse->pSTranscoder,outbf);

    p_instanse->pSTranscoder->pInSample->pRect=NULL;

	struct timeval tm;
	struct timeval tm1;
	gettimeofday(&tm,NULL);
	int iloop = 3;
	while(!HasCode_DataFlush(p_instanse->pSTranscoder))
	{
		usleep(5);
		if(iloop-- < 0)
			break;
	}

	if(iloop < 0)
	{
		gettimeofday(&tm1,NULL);
		long start_time_us,end_time_us;
		start_time_us  = tm.tv_sec*1000*1000 + tm.tv_usec;
		end_time_us = tm1.tv_sec*1000*1000 + tm1.tv_usec;
		printf("libvideoencoder:WARN:: Encoding elapsed %ld(us)\n",end_time_us-start_time_us);
	}
	struct coded_buff cdbf={0};
	
//    cdbf.buff=(unsigned char*)output_video_sample;
    cdbf.buff=(unsigned char*)p_instanse->buff_tmp;
	//cdbf.length=p_instanse->pSTranscoder->pOutSample->lBufSize;
	if(GetCode_Data(p_instanse->pSTranscoder,&cdbf)< 0)
	{
		fprintf(stderr ,"libvideoencoder:WARN:: get code data failed..\n");		
		return -3;	 
	}



	int padding = 512 - cdbf.length;
	if(padding < 0)
		padding = 0;

	p_instanse->real_count_fps++;

	if(*output_video_sample_size < cdbf.length + padding)
	{		 
		fprintf(stderr ,"libvideoencoder: Error output_video_sample_size is too small..\n");		
		return -3;	  
	} 
	
	memcpy(output_video_sample,cdbf.buff,cdbf.length);
	memset(output_video_sample + cdbf.length,0,padding);
	*output_video_sample_size = cdbf.length + padding;

//    fprintf(stderr,"End encode inc\n");

	return 0;
}

unsigned long get_real_count_fps(video_encoder_t *videoencoder)
{
	if(videoencoder == NULL)
	{		 
		fprintf(stderr ,"libvideoencoder: Error paraments..\n");		
		return -1;	  
	}
	videoencoder_instanse *p_instanse = (videoencoder_instanse *)videoencoder;
	return p_instanse->real_count_fps;
}

int uninit_video_encoder(video_encoder_t *videoencoder)
{
	if(videoencoder == NULL)
	{		 
		fprintf(stderr ,"libvideoencoder: Error paraments..\n");		
		return -1;	  
	}
	videoencoder_instanse *p_instanse = (videoencoder_instanse *)videoencoder;

    free(p_instanse->rect);
	free(p_instanse->outbf);
	free(p_instanse->buff_tmp);
	close_encoder(p_instanse->pSTranscoder);
	free(p_instanse);

	return 0;
}




