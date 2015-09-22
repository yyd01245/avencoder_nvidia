#include "videoencoder.h"
//#include "encodec_nv/encodenv.h"

#include <stdint.h>
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include<fcntl.h>
#include<sys/types.h>
#include <errno.h>
#include <dlfcn.h>


#if 0
typedef struct{
    int flag;
    int x;
    int y;
    int w;
    int h;

}enc_rect_t;



typedef struct{
    
    unsigned char*input;
    unsigned char*output;
    unsigned char*prect;
    int output_siz;
    int flag_inc;
}nv_io_t;
#endif
typedef struct _enc_rect{

    unsigned int x;//left
    unsigned int y;//top
    unsigned int w;//width
    unsigned int h;//height

}rect;
typedef struct
{
    uint32_t Data1;                                      /**< [in]: Specifies the first 8 hexadecimal digits of the GUID.                                */
    uint16_t Data2;                                      /**< [in]: Specifies the first group of 4 hexadecimal digits.                                   */
    uint16_t Data3;                                      /**< [in]: Specifies the second group of 4 hexadecimal digits.                                  */
    uint8_t  Data4[8];                                   /**< [in]: Array of 8 bytes. The first 2 bytes contain the third group of 4 hexadecimal digits.
                                                                    The remaining 6 bytes contain the final 12 hexadecimal digits.                       */
} GUID;

typedef struct _EncodeConfig
{
    int              width;
    int              height;
    int              maxWidth;
    int              maxHeight;
    int              fps;
    int              bitrate;
    int              vbvMaxBitrate;
    int              vbvSize;
    int              rcMode;
    int              qp;
    GUID             profileGUID;
    GUID             presetGUID;
//    FILE            *fOutput;
    int              codec;
    int              invalidateRefFramesEnableFlag;
    int              intraRefreshEnableFlag;
    int              intraRefreshPeriod;
    int              intraRefreshDuration;
    int              deviceType;
    int              gopsize0;//for startup//force IDR
    int              gopsize;//for idrperiod
    int              gopLength;
    int              numB;
    int              pictureStruct;
    int              deviceID;
    int              isYuv444;
    char            *qpDeltaMapFile;
	int		 		repeatSPSPPS;

    char* encoderPreset;
    char* encoderProfile;
}EncodeConfig;

typedef struct _NvEncPictureCommand
{
    bool bResolutionChangePending;
    bool bBitrateChangePending;
    bool bForceIDR;
    bool bForceIntraRefresh;
    bool bInvalidateRefFrames;

    uint32_t newWidth;
    uint32_t newHeight;

    uint32_t newBitrate;
    uint32_t newVBVSize;

    uint32_t  intraRefreshDuration;

    uint32_t  numRefFramesToInvalidate;
    uint32_t  refFrameNumbers[16];
}NvEncPictureCommand;




typedef struct _nvEncoder{
	void* phwEncoder;
	EncodeConfig encCfg;
	NvEncPictureCommand picCmd;
}nvEncodeHandle;



//int nv_encoder_init(nv_encoder_struct* ,nv_encoder_config*);
typedef int (*pNv_encoder_init)(nvEncodeHandle *enc,char argc,char**argv);

//int nv_encoder_fini(nv_encoder_struct*);
typedef int (*pNv_encoder_fini)(nvEncodeHandle*enc);

//int nv_encoder_encodeframe(nv_encoder_struct*pencoder,const char*src,int srcsiz,char*bitstream,int*bssiz,rect*encrect);
typedef int (*pNv_encoder_encodeframe)(nvEncodeHandle*enc,char*nv12,char*oes);

//void nv_encoder_get_default_config(nv_encoder_config*config);
typedef void (*pNv_encoder_get_default_config)(EncodeConfig *encCfg);





typedef struct
{
	int width;
	int height;
	int gopsize_min;
	int gopsize_max;
	int bitrate;//bps
	int fps;

    int devid;//gpu core
//	unsigned char *outbf;
//	unsigned char *enc_buf;
//	unsigned char *rgb_buffer;
//    unsigned char* rgb_shm;
//    unsigned char* shm_addr;
//    enc_rect_t shm_rect;
	//for statistics
	unsigned long real_count_fps;

  //  nv_encoder_struct nvencoder;

	nvEncodeHandle *nvencoder;

	char *m_yuv;

	pNv_encoder_init 				m_pfnv_encoder_init;
	pNv_encoder_fini 				m_pfnv_encoder_fini;
	pNv_encoder_encodeframe 		m_pfnv_encoder_encodeframe;
	pNv_encoder_get_default_config	m_pfnv_encoder_get_default_config;

	void *m_handle;
//    nv_io_t nvio;
}videoencoder_instanse;



int LoadAPI(videoencoder_instanse *p_instanse )
{
	
	//µ¼Èë¶¯Ì¬¿â
	void *hEncoder = dlopen("/usr/local/lib/libnvencoder.so", RTLD_LAZY);	
	if (hEncoder == NULL)
	{	
		fprintf(stderr,"load libnvencoder.so failed %s \n",strerror(errno));
		return -1;	
	}
	p_instanse->m_handle = NULL;
	p_instanse->m_pfnv_encoder_encodeframe = NULL;
	p_instanse->m_pfnv_encoder_fini = NULL;
	p_instanse->m_pfnv_encoder_get_default_config = NULL;
	p_instanse->m_pfnv_encoder_init = NULL;
	
	p_instanse->m_handle = hEncoder;
	p_instanse->m_pfnv_encoder_init = (pNv_encoder_init)dlsym(hEncoder, "NvEncoder_Initialize");
	p_instanse->m_pfnv_encoder_encodeframe = (pNv_encoder_encodeframe)dlsym(hEncoder, "NvEncoder_EncodeFrame");
	p_instanse->m_pfnv_encoder_get_default_config = (pNv_encoder_get_default_config)dlsym(hEncoder, "NvEncoder_InitDefaultConfig");
	p_instanse->m_pfnv_encoder_fini = (pNv_encoder_fini)dlsym(hEncoder,"NvEncoder_Finalize");

	if(p_instanse->m_pfnv_encoder_encodeframe==NULL || NULL== p_instanse->m_pfnv_encoder_fini
		|| p_instanse->m_pfnv_encoder_init== NULL|| p_instanse->m_pfnv_encoder_get_default_config ==NULL)
	{
		fprintf(stderr,"load libencodenv.so API Func failed \n");
		return -1;	
	}
	return 0;

}




video_encoder_t* init_video_encoder(InputParams_videoencoder *pInputParams)
{
	if(pInputParams == NULL)
	{        
		fprintf(stderr ,"libvideoencoder: Error paraments..\n");        
		return NULL;    
	}

	
	videoencoder_instanse *p_instanse = (videoencoder_instanse *)calloc(1,sizeof(videoencoder_instanse));
	if(p_instanse == NULL)
	{        
		fprintf(stderr ,"libvideoencoder: Error malloc..\n");        
		return NULL;    
	}
	int iret = LoadAPI(p_instanse);
	if(iret<0)
	{
		fprintf(stderr ,"libvideoencoder: Error malloc..\n");        
		return NULL;  

	}

	p_instanse->width = pInputParams->width;
	p_instanse->height = pInputParams->height;
	p_instanse->gopsize_min = pInputParams->gopsize_min;
	p_instanse->gopsize_max = pInputParams->gopsize_max;
	p_instanse->fps = pInputParams->fps;
	p_instanse->bitrate = pInputParams->bitrate/1024;
	p_instanse->real_count_fps = 0;
    p_instanse->devid=pInputParams->devid;
#if 0
    int val=0;
    char buff[10]={0,};
    int fd=open("idid",O_CREAT|O_RDWR,0666);
    int nr=read(fd,buff,10);
    if(nr==0){
        val=0;
    }else{
        val=atoi(buff);
    }
    int l=snprintf(buff,sizeof(buff),"%d",(val+1)%3);
    lseek(fd,0,SEEK_SET);
    write(fd,buff,l);
    close(fd);
    p_instanse->devid=val;
#endif

    p_instanse->devid=-1;//auto select


//    p_instanse->shm_addr=pInputParams->shm_addr;
//	p_instanse->outbf = (unsigned char *)malloc(p_instanse->width*p_instanse->height*2);
//	p_instanse->enc_buf = (unsigned char *)malloc(p_instanse->width*p_instanse->height*2);
//	p_instanse->rgb_buffer = (unsigned char *)malloc(p_instanse->width*p_instanse->height*4);
//	if(p_instanse->outbf == NULL)
//	{        
//		fprintf(stderr ,"libvideoencoder: Error malloc..\n");        
//		return NULL;    
//	}
/////////////////////////////////////////////////////////
//
#if 0
    nv_encoder_config nvconfig;
//    nv_encoder_struct nvencoder;

    nv_encoder_get_default_config(&nvconfig);

    nvconfig.device_id=p_instanse->devid;
    nvconfig.width=p_instanse->width;
    nvconfig.height=p_instanse->height;
    nvconfig.idr_period_startup=p_instanse->gopsize_min;
    nvconfig.idr_period=p_instanse->gopsize_max;
    //TODO
    nvconfig.frame_rate_num=p_instanse->fps;
    nvconfig.frame_rate_den=1;
    //

//    nvconfig.conv_type=1;//0:cuda 1:host


	if(p_instanse->width<=720)
		p_instanse->bitrate=4000;
	else if(p_instanse->width<=1280 && p_instanse->width>720 )
		p_instanse->bitrate=4000;
	else if(p_instanse->width>1280)
		p_instanse->bitrate=9000;

    nvconfig.avg_bit_rate=p_instanse->bitrate*1024;
    nvconfig.peak_bit_rate=0;


	fprintf(stderr ,"libvideoencoder: Init H.264 encoder..\n");
    nv_encoder_init(&p_instanse->nvencoder,&nvconfig);
#endif
	//new lib
	
	p_instanse->nvencoder= (nvEncodeHandle *)malloc(sizeof(nvEncodeHandle));

	memset(p_instanse->nvencoder,0,sizeof(p_instanse->nvencoder));
	p_instanse->m_pfnv_encoder_get_default_config(&p_instanse->nvencoder->encCfg);

	p_instanse->nvencoder->encCfg.deviceID=p_instanse->devid;
	p_instanse->nvencoder->encCfg.width=p_instanse->width;
	p_instanse->nvencoder->encCfg.height=p_instanse->height;
	p_instanse->nvencoder->encCfg.gopsize=p_instanse->gopsize_max;
	p_instanse->nvencoder->encCfg.gopsize0=p_instanse->gopsize_min;
	//TODO
	//enc->encCfg.intraRefreshEnableFlag=1;
	p_instanse->nvencoder->encCfg.intraRefreshPeriod=p_instanse->nvencoder->encCfg.gopsize;
 //   nvencoder->encCfg.intraRefreshDuration=4; 
//	nvencoder->encCfg.repeatSPSPPS = 1;
	
	p_instanse->nvencoder->encCfg.fps=p_instanse->fps;

	if(p_instanse->width<=720)
		p_instanse->bitrate=4000;
	else if(p_instanse->width<=1280 && p_instanse->width>720 )
		p_instanse->bitrate=4000;
	else if(p_instanse->width>1280)
		p_instanse->bitrate=9000;

	p_instanse->nvencoder->encCfg.bitrate=p_instanse->bitrate*1024;

	p_instanse->m_yuv = (char*)malloc(p_instanse->width*p_instanse->height*3/2);

	fprintf(stderr ,"libvideoencoder: Init H.264 encoder..\n");
	p_instanse->m_pfnv_encoder_init(p_instanse->nvencoder,0,NULL);

	return p_instanse;
}

void load_rgb_bgrx_ippcc_patch(unsigned char*yuv,unsigned char*rgb,
        int left,int top,int width,int height,//patch rectangle
        int rgbheight,
        int ostride)
{
#if 0 
    static int icnt;
    if(icnt++%100==0)
        fprintf(stderr,"RGB<%p> , YUV<%p>.{%d,%d}\n",rgb,yuv,ostride,rgbheight);
#endif 
	//fprintf(stderr,"--x=%d,y=%d,w=%d,h=%d,rgbheight=%d,ostride=%d \n",left,top,width,height,rgbheight,ostride);
    static unsigned char p2[1920*1080];//temp

    int _width=ostride;
    int _height=rgbheight;

    unsigned char *pDst=yuv;
    unsigned char*pDst2[3];
    pDst2[0]=yuv+top*_width+left;
    pDst2[1]=(unsigned char*)p2;//yuv+rgbheight*ostride;
    pDst2[2]=(unsigned char*)p2+((_width*_height)>>2);

    int pDstStep[3];
    pDstStep[0]=_width;
    pDstStep[1]=_width>>1;
    pDstStep[2]=_width>>1;

    IppiSize roiSize;
    roiSize.width=width;
    roiSize.height=height;

    int chromaoff=_width*_height;

    IppStatus ippret;

    ippret=ippiBGRToYCbCr420_709CSC_8u_AC4P3R(rgb,width*4,pDst2,pDstStep,roiSize);
    if(ippret!=ippStsNoErr){
        fprintf(stderr,"Convert RGB to YUV Failed..\n");
    }

    int width_2=width>>1;
    int height_2=height>>1;
    int _width_2=_width>>1;
#if 1
    unsigned char*pline;
    int i; int j;
    for(i=0;i<height_2;i++){
        pline=yuv+chromaoff+((top>>1)+i)*_width;
        int ioff=i*_width_2;
        for(j=0;j<width_2;j++){
            int j2=j<<1;
            pline[left+j2  ]=pDst2[1][ioff+j];
            pline[left+j2+1]=pDst2[2][ioff+j];
        }  
    }
#endif
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

    int ret;

    rect r;
    r.x=r.y=0;
    r.w=1280;
    r.h=720;
	
	load_rgb_bgrx_ippcc_patch((unsigned char*)p_instanse->m_yuv,(unsigned char*)const_cast< char*>(input_video_sample),
		r.x,r.y,r.w,r.h,//patch rectangle
		p_instanse->height,p_instanse->width);

	*output_video_sample_size =p_instanse->m_pfnv_encoder_encodeframe(p_instanse->nvencoder,p_instanse->m_yuv,output_video_sample);

	int iLength = *output_video_sample_size;
	int padding = 2048 - iLength;
	if(padding < 0)
		padding = 0;

	if(padding > 0)
	{
		//printf("---padding len =%d src len=%d\n",padding,iLength);
		memset(output_video_sample + iLength,0,padding);
		*output_video_sample_size = iLength + padding ;
	}
	//printf("---encode len =%d \n",*output_video_sample_size);
//	if(NULL==fp)
//		fp =fopen("dd.h264","wb");
//	fwrite(output_video_sample,1,*output_video_sample_size,fp);
//	  usleep(100);

	p_instanse->real_count_fps++;


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
	videoencoder_instanse *p_instanse = (videoencoder_instanse *)videoencoder;

    int ret;


    rect r;
    r.x=x;
    r.y=y;
    r.w=w;
    r.h=h;
    
	
	if(!(r.w==0 || r.h==0))
		load_rgb_bgrx_ippcc_patch((unsigned char*)p_instanse->m_yuv,(unsigned char*)const_cast<char*>(input_video_sample),
			r.x,r.y,r.w,r.h,//patch rectangle
			p_instanse->height,p_instanse->width);

	*output_video_sample_size =p_instanse->m_pfnv_encoder_encodeframe(p_instanse->nvencoder,p_instanse->m_yuv,output_video_sample);

	int iLength = *output_video_sample_size;
	int padding = 2048 - iLength;
	if(padding < 0)
		padding = 0;

	if(padding > 0)
	{
		//printf("---padding len =%d src len=%d\n",padding,iLength);
		memset(output_video_sample + iLength,0,padding);
		*output_video_sample_size = iLength + padding ;
	}

	//printf("---encode len =%d \n",*output_video_sample_size);
//	if(NULL==fp)
//		fp =fopen("dd.h264","wb");
//	fwrite(output_video_sample,1,*output_video_sample_size,fp);
//	  usleep(100);

	//printf("x=%d,y=%d,w=%d,h=%d\n",x,y,w,h);
	p_instanse->real_count_fps++;

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

 
	 p_instanse->m_pfnv_encoder_fini(p_instanse->nvencoder);
	 
	 free(p_instanse->nvencoder);
	 p_instanse->nvencoder =NULL;


 //   nv_encoder_fini(&p_instanse->nvencoder);

//	free(p_instanse->outbf);
//	free(p_instanse->enc_buf);
	free(p_instanse);

//	close_encoder();


	return 0;
}




