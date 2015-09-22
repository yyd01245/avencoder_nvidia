#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<pthread.h>
#include<sys/mman.h>
#include<fcntl.h>
#include<sys/types.h>
#include<signal.h>

#include<cuda.h>
#include"nvEncodeAPI.h"
#include"helper_nvenc.h"
#include"drvapi_error_string.h"
#include"formats.h"
#include"encodenv.h"



typedef int bool;

#define TRUE 1
#define FALSE 0


#ifdef _DEBUG_

void sig_handler(int signo);

#endif

typedef enum _NvEncodeCodecType{

    NV_ENC_CODEC_UNKNOW=0,
    NV_ENC_CODEC_H264

}NvEncCodecType;

typedef enum _NvEncodeStart{

    NV_ENC_STARTUP_NORMAL=0,
    NV_ENC_STARTUP_SLOW

}NvEncodeStartup;

typedef enum _NvEncodeInterfaceType{

    NV_ENC_IFACE_NORMAL=0,
    NV_ENC_IFACE_CUDA

}NvEncIfaceType; 


typedef enum _NvEncodeInputType{

    NV_ENC_INPUT_NULL=0,
    NV_ENC_INPUT_YV12=1<<0,
    NV_ENC_INPUT_NV12=1<<1,//2,
    NV_ENC_INPUT_BGRX=1<<2,//4,

    NV_ENC_INPUT_CUDA_CONV=1<<3,//8,
    NV_ENC_INPUT_HOST_CONV=1<<4,//16,

    NV_ENC_INPUT_CREATE_BUFFER=1<<5,//32,
    NV_ENC_INPUT_USE_SHM=1<<6 //64
    //CREATE INPUTDATA..See Below 

}NvEncInputType; 

/*
    
    INPUTDATA -> DATAHOST -> DATADEVICE [HOST Conversion]
    
    INPUTDATA -> DATADEVICETMP -> DATADEVICE {CUDA Conversion}

    SHMDATA -> DATADEVICETMP -> DATADEVICE 

*/

typedef struct _ENC_CONFIG{

    NvEncCodecType              codec;//Only H264 allowed
    unsigned int                profile;
    unsigned int                level;
    unsigned int                width;
    unsigned int                height;
    unsigned int                maxWidth;
    unsigned int                maxHeight;
    unsigned int                frameRateNum;
    unsigned int                frameRateDen;
    unsigned int                avgBitRate;
    unsigned int                peakBitRate;
    unsigned int                gopLength;
    unsigned int                enableInitialRCQP;
    NV_ENC_QP                   initialRCQP;
    unsigned int                numBFrames;
    unsigned int                fieldEncoding;
    unsigned int                bottomFieldFrist;
    unsigned int                rateControl; // 0= QP, 1= CBR. 2= VBR
    int                         numSlices;
    unsigned int                vbvBufferSize;
    unsigned int                vbvInitialDelay;
    NV_ENC_MV_PRECISION         mvPrecision;
    unsigned int                enablePTD;
    int                         preset;
    int                         syncMode;
    NvEncIfaceType              iface_type;


#ifndef MAPPED_ONLY
    unsigned int                useMappedResources;
#endif

    unsigned int                chromaFormatIDC;//1 for YUV420 ;3 for YUV444
    unsigned int                separateColourPlaneFlag;
    unsigned int                enableAQ;
    unsigned int                intraRefreshCount;
    unsigned int                intraRefreshPeriod;
    unsigned int                idr_period;
    unsigned int                idr_period_startup;//adopted when frame_count <=idr_period
    unsigned int                vle_cabac_enable;
    unsigned int                sliceMode;
    unsigned int                sliceModeData;

    unsigned int                device_id;

    NV_ENC_BUFFER_FORMAT        input_fmt;//ALWAYS SET NV12;Only Nv12 Supported.
    NvEncInputType              input_type;

    unsigned int                num_frames;

    void*                       shm_data;


}ENC_CONFIG;



enum
{
    NV_ENC_CODEC_PROFILE_AUTOSELECT        = 0,
    NV_ENC_H264_PROFILE_BASELINE           = 1,
    NV_ENC_H264_PROFILE_MAIN               = 2,
    NV_ENC_H264_PROFILE_HIGH               = 3,
    NV_ENC_H264_PROFILE_HIGH_444           = 4,
    NV_ENC_H264_PROFILE_STEREO             = 5,
    NV_ENC_H264_PROFILE_SVC = 6,
//==    NV_ENC_H264_PROFILE_SVC_TEMPORAL_SCALABILTY;
    NV_ENC_H264_PROFILE_CONSTRAINED_HIGH   = 7

};


enum
{
    NV_ENC_PRESET_DEFAULT                   =0,
    NV_ENC_PRESET_LOW_LATENCY_DEFAULT       =1,
    NV_ENC_PRESET_HP                        =2,
    NV_ENC_PRESET_HQ                        =3,
    NV_ENC_PRESET_BD                        =4,
    NV_ENC_PRESET_LOW_LATENCY_HQ            =5,
    NV_ENC_PRESET_LOW_LATENCY_HP            =6,
    NV_ENC_PRESET_LOSSLESS_DEFAULT          =8,
    NV_ENC_PRESET_LOSSLESS_HP               =9
};

typedef enum _H264SliceMode
{
    H264_SLICE_MODE_MBS       = 0x0,
    H264_SLICE_MODE_BYTES     = 0x1,
    H264_SLICE_MODE_MBROW     = 0x2,
    H264_SLICE_MODE_NUMSLICES = 0x3,
}H264SliceMode;


static void _init_enc_config(ENC_CONFIG*econf)
{

    econf->codec =NV_ENC_CODEC_H264;

    econf->rateControl =NV_ENC_PARAMS_RC_CBR;

    //set below values to 0;Fix Startup's Low Quality of Iframe..
    econf->vbvBufferSize = 2000000;
    econf->vbvInitialDelay = 1000000;

    econf->avgBitRate =3000000;
    econf->peakBitRate=econf->avgBitRate>>1+econf->avgBitRate;

    econf->idr_period =25;
    econf->gopLength =25;

    econf->frameRateNum =25;
    econf->frameRateDen =1;

    econf->width =0;
    econf->height=0;
    econf->maxWidth =econf->width;
    econf->maxHeight=econf->height;

    econf->preset = NV_ENC_PRESET_DEFAULT;//0
    econf->profile = NV_ENC_CODEC_PROFILE_AUTOSELECT; //0
    econf->level =NV_ENC_LEVEL_AUTOSELECT;//0


    //Do not Change
    econf->iface_type =NV_ENC_IFACE_CUDA;//
    //Do not Modify follow line
    econf->input_fmt=NV_ENC_BUFFER_FORMAT_NV12_PL;

//    econf->input_type|=NV_ENC_INPUT_CREATE_BUFFER;
//    econf->input_type|=NV_ENC_INPUT_CUDA_CONV;
//    econf->input_type|=NV_ENC_INPUT_YV12;

    econf->mvPrecision =NV_ENC_MV_PRECISION_QUARTER_PEL;

    econf->enablePTD = 1;


    econf->numBFrames=0;//FIXME :not support Bframes.

    econf->numSlices=0;


#ifndef MAPPED_ONLY
    econf->useMappedResources =1;//1
#endif

 
    econf->syncMode =1;
    econf->num_frames =0;
    
    econf->chromaFormatIDC =1;
    econf->separateColourPlaneFlag =0;
    econf->enableAQ =0;

    econf->intraRefreshCount =0;
    econf->intraRefreshPeriod =30;

    econf->vle_cabac_enable =0;
    econf->fieldEncoding =0;
    econf->sliceMode =0;//H264_SLICE_MODE_NUMSLICES;
    econf->sliceModeData =0;//1;

    econf->device_id=0;//1;
}



void merge_config(ENC_CONFIG*conf,nv_encoder_config*pubconf)
{

    conf->num_frames=pubconf->num_frames; // only encode `num_frame` frames,or less

    conf->width=pubconf->width;
    conf->height=pubconf->height;
    conf->maxWidth=conf->width;
    conf->maxHeight=conf->height;

    //30000/1001=29.97; 24000/1001=23.97 60000/1001=59.94; 25/1=25; 30/1=30 and etc
    conf->frameRateNum = pubconf->frame_rate_num;
    conf->frameRateDen = pubconf->frame_rate_den;

    conf->rateControl = pubconf->rate_control;


    conf->avgBitRate=pubconf->avg_bit_rate;//average bit rate
    if(pubconf->peak_bit_rate>pubconf->avg_bit_rate)
        conf->peakBitRate=pubconf->peak_bit_rate;//max bit rate
    else
        conf->peakBitRate=pubconf->avg_bit_rate*1.5;//max bit rate

    pubconf->gop_length=25;
    fprintf(stderr,"\033[32mForce Gopsize :%d \033[0m\n",pubconf->gop_length);


    //gop_length=idr_period;
    conf->gopLength=pubconf->gop_length;//idr_interval 
    conf->idr_period=conf->gopLength;//idr_interval 
    if(pubconf->idr_period_startup<=0){
//        conf->startup_type=NV_ENC_STARTUP_NORMAL;
        conf->idr_period_startup=0;
    }else{
//        conf->startup_type=NV_ENC_STARTUP_SLOW;
        conf->idr_period_startup=pubconf->idr_period_startup;
        fprintf(stderr,"\033[32mSlow Startup{%d:%d} \033[0m\n",conf->idr_period_startup,conf->idr_period);
    }

/*FIXME:Not support fieldencoding now
    conf->fieldEncoding=pubconf->field_encoding;//=0 frame encodeing
    if(conf->fieldEncoding)
        conf->bottomFieldFrist=pubconf->bottom_field_first;
*/
    conf->input_fmt=NV_ENC_BUFFER_FORMAT_NV12_PL;

    switch(pubconf->input_type)//1:yv12 2:nv12 3:bgrx
    {
        case 1:
            conf->input_type|=NV_ENC_INPUT_YV12;
            break;

        case 2:
            conf->input_type|=NV_ENC_INPUT_NV12;
            break;

        case 3:
            conf->input_type|=NV_ENC_INPUT_BGRX;
            break;

        default:
            fprintf(stderr,"WARN::Input Type not set,Using NV12 default..\n");
            conf->input_type|=NV_ENC_INPUT_NV12;
            
    }

//    if(pubconf->input_type&NV_ENC_INPUT_USE_SHM){
    if(pubconf->shm_addr){
        conf->shm_data=pubconf->shm_addr;
        conf->input_type|=NV_ENC_INPUT_USE_SHM;
        fprintf(stderr,"\033[32m Use shm input\033[0m\n");
    }else{//NV_ENC_INPUT_CREATE_BUFFER;
        conf->input_type|=NV_ENC_INPUT_CREATE_BUFFER;
        conf->shm_data=NULL;
    }

    switch(pubconf->conv_type){
        
        case 0:
            conf->input_type|=NV_ENC_INPUT_CUDA_CONV;
            break;

        case 1:
            conf->input_type|=NV_ENC_INPUT_HOST_CONV;
            break;

        default:
            fprintf(stderr,"WARN::Input Convert Type not set,Using HOST_CONVERT default..\n");
            conf->input_type|=NV_ENC_INPUT_HOST_CONV;

    }

    conf->device_id=pubconf->device_id;

}









typedef struct _EncInputStruct{

    unsigned int      dwWidth;
    unsigned int      dwHeight;
    unsigned int      dwLumaOffset;
    unsigned int      dwChromaOffset;
    void              *hInputSurface;
    unsigned int      lockedPitch;
    NV_ENC_BUFFER_FORMAT bufferFmt;
    NvEncInputType    inputType;
    void*             *pExtAllocTmp;//For Format Conversion
    unsigned int      dwCuPitch_conv;
    void              *pExtAlloc;//For Encode Input
    unsigned char     *pExtAllocHost;//TODO remove this buffer
    unsigned int      dwCuPitch;
    NV_ENC_INPUT_RESOURCE_TYPE type;
    void              *hRegisteredHandle;

}ENC_INPUT_INFO;



typedef void* HANDLE;

typedef  struct _EncOutputInfo{

    unsigned int     dwSize;
    unsigned int     dwBitstreamDataSize;
    void             *hBitstreamBuffer;
    HANDLE           hOutputEvent;
    bool             bWaitOnEvent;
    void             *pBitstreamBufferPtr;
    bool             bEOSFlag;
    bool             bReconfiguredflag;

}ENC_OUTPUT_INFO;




typedef struct _enc_rect{

    unsigned int x;//left
    unsigned int y;//top
    unsigned int w;//width
    unsigned int h;//height

}rect;





typedef struct _EncFrameConf{

    unsigned int width;
    unsigned int height;
    unsigned int stride[3];
    unsigned char* yuv[3];
    rect*       enc_rect;
    NV_ENC_PIC_STRUCT picStruct;//NV_ENC_PIC_STRUCT_FRAME
    bool bReconfigured;//Be Reconfigured
    NvEncodeStartup startup_type;
    int startup_count;
}ENC_FRAME_CONFIG;





///////////////////////////////////////////////////////////////////////////////



static GUID get_profile_by_index(int index)
{


    GUID guid;

    switch(index){

        case NV_ENC_CODEC_PROFILE_AUTOSELECT:
            guid=NV_ENC_CODEC_PROFILE_AUTOSELECT_GUID;
            break;
        case NV_ENC_H264_PROFILE_BASELINE:
            guid=NV_ENC_H264_PROFILE_BASELINE_GUID;
            break;
        case NV_ENC_H264_PROFILE_MAIN:
            guid=NV_ENC_H264_PROFILE_MAIN_GUID;
            break;
        case NV_ENC_H264_PROFILE_HIGH:
            guid=NV_ENC_H264_PROFILE_HIGH_GUID;
            break;
        case NV_ENC_H264_PROFILE_HIGH_444:
            guid=NV_ENC_H264_PROFILE_HIGH_444_GUID;
            break;
        case NV_ENC_H264_PROFILE_STEREO:
            guid=NV_ENC_H264_PROFILE_STEREO_GUID;
            break;
        case NV_ENC_H264_PROFILE_SVC:
            guid=NV_ENC_H264_PROFILE_SVC_TEMPORAL_SCALABILTY;
            break;
        case NV_ENC_H264_PROFILE_CONSTRAINED_HIGH:
            guid=NV_ENC_H264_PROFILE_CONSTRAINED_HIGH_GUID;
            break;

        default:

            guid=NV_ENC_CODEC_PROFILE_AUTOSELECT_GUID;

    }

    return guid;

}


static GUID get_preset_by_index(int index)
{

    GUID guid;

    switch(index){
        

        case NV_ENC_PRESET_DEFAULT            :       
            guid=NV_ENC_PRESET_DEFAULT_GUID;
            break;
        case NV_ENC_PRESET_LOW_LATENCY_DEFAULT    :       
            break;
        case NV_ENC_PRESET_HP                     :       
            guid=NV_ENC_PRESET_HP_GUID;
            break;
        case NV_ENC_PRESET_HQ                     :       
            guid=NV_ENC_PRESET_HQ_GUID;
            break;
        case NV_ENC_PRESET_BD                     :       
            guid=NV_ENC_PRESET_BD_GUID;
            break;
        case NV_ENC_PRESET_LOW_LATENCY_HQ         :       
            guid=NV_ENC_PRESET_LOW_LATENCY_HQ_GUID;
            break;
        case NV_ENC_PRESET_LOW_LATENCY_HP         :       
            guid=NV_ENC_PRESET_LOW_LATENCY_HP_GUID;
            break;
        case NV_ENC_PRESET_LOSSLESS_DEFAULT       :       
            guid=NV_ENC_PRESET_LOSSLESS_DEFAULT_GUID;
            break;
        case NV_ENC_PRESET_LOSSLESS_HP            :       
            guid=NV_ENC_PRESET_LOSSLESS_HP_GUID;
            break;

        default:
            guid=NV_ENC_PRESET_DEFAULT_GUID;



    }

    return guid;

}






static bool compareGUIDs(GUID guid1, GUID guid2)
{
    if (guid1.Data1    == guid2.Data1 &&
        guid1.Data2    == guid2.Data2 &&
        guid1.Data3    == guid2.Data3 &&
        guid1.Data4[0] == guid2.Data4[0] &&
        guid1.Data4[1] == guid2.Data4[1] &&
        guid1.Data4[2] == guid2.Data4[2] &&
        guid1.Data4[3] == guid2.Data4[3] &&
        guid1.Data4[4] == guid2.Data4[4] &&
        guid1.Data4[5] == guid2.Data4[5] &&
        guid1.Data4[6] == guid2.Data4[6] &&
        guid1.Data4[7] == guid2.Data4[7])
    {
        return 1;
    }

    return 0;
}





/*First Step.*/

NV_ENCODE_API_FUNCTION_LIST* init_encode_api(void)
{

    NV_ENCODE_API_FUNCTION_LIST*encapi;
    NVENCSTATUS nvret;
    encapi=calloc(1,sizeof(NV_ENCODE_API_FUNCTION_LIST));
    if(!encapi)
        return NULL;
    encapi->version=NV_ENCODE_API_FUNCTION_LIST_VER; 

    nvret=NvEncodeAPICreateInstance(encapi);
    //<CHK>
    CHK_NVENC_ERR(nvret);

    return encapi;
}

void fini_encode_api(NV_ENCODE_API_FUNCTION_LIST*encapi)
{
    if(!encapi)
        return;

    free(encapi);

}



typedef struct _Encoder_Main{

    NV_ENCODE_API_FUNCTION_LIST*encapi;

    void*handler;

    nv_encoder_struct*caller;

    ENC_CONFIG *config;


    unsigned int dwframe_num_in_gop;


    NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS st_enc_session_par;
//    NV_ENC_CAPS_PARAM caps_pars;
//    GUID client_guid;
    GUID st_encode_guid;
    unsigned    int nb_encode_guid;
    
    GUID st_preset_guid;
     unsigned    int nb_preset_guid;
    NV_ENC_PRESET_CONFIG st_preset_config;
    
    GUID st_profile_guid;
    unsigned    int nb_profile_guid;

    NV_ENC_CONFIG st_encode_config;

    unsigned    int nb_input_fmt;
    NV_ENC_BUFFER_FORMAT st_input_fmt;

    NV_ENC_INITIALIZE_PARAMS st_init_enc_pars;
//    NV_ENC_RECONFIGURE_PARAMS st_reconf_enc_pars;



    ENC_FRAME_CONFIG st_frame_conf;

    ENC_INPUT_INFO input;
    ENC_OUTPUT_INFO output;

//    NV_ENC_PIC_PARAMS st_pic_pars;

//    int b_encoder_initialized;

    CUcontext st_cu_ctx;
//////////////////////////
//

    unsigned int st_frame_width;
    unsigned int st_frame_height;
    unsigned int st_max_width;
    unsigned int st_max_height;


    unsigned char* st_yuv[3];

    int b_async_mode;

#if 0
//====
    unsigned  int st_poc;
    unsigned  int st_frame_num_syntax;
    unsigned  int st_idr_period;
    unsigned  int st_frame_num_in_gop;
//===
#endif

    unsigned int st_luma_siz;
    unsigned int st_chroma_siz;

//    FILE* ofp;//fd
//    FILE* ifp;


    void*io_data;

    nv_encoder_read_data st_reader;
    nv_encoder_write_data st_writer;
//    nv_encoder_flush_data _flush;

    pthread_t thrid;
    pthread_attr_t attr;


    int b_encode;
    pthread_mutex_t mutex;
    pthread_cond_t cond;

    int st_current_frame;

    rect st_rect;



    //profile performance
    
    struct timespec ts0;
    struct timespec ts1;



}ENCODER;


///////////////////////////////
// Thread stuff
typedef ENCODER ThrArgs;


#ifdef _DEBUG_
//Global ENCODER; for reference
static ENCODER*global_enc_ref;
#endif


void _frame_conf_set_head(nv_encoder_struct*pencoder,int x,int y,int w,int h)
{
    if(!pencoder)
        return;

    ENCODER*enc=(ENCODER*)pencoder->dummy[0];

    enc->st_frame_conf.enc_rect->x=x;
    enc->st_frame_conf.enc_rect->y=y;
    enc->st_frame_conf.enc_rect->w=w;
    enc->st_frame_conf.enc_rect->h=h;

}


void nv_encoder_set_io_data(nv_encoder_struct*pencoder,void*iodata)
{
    if(!pencoder)
        return;

    ENCODER*enc=(ENCODER*)pencoder->dummy[0];

    if(enc->io_data)
        free(enc->io_data);

    enc->io_data=iodata;
}



void* nv_encoder_get_io_data(nv_encoder_struct*pencoder)
{
    if(pencoder){
        ENCODER*enc=(ENCODER*)pencoder->dummy[0];
        return enc->io_data;
    }
    return NULL;
}



typedef struct _stdio_mgr{

    FILE*ifp;
    FILE*ofp;

}stdio_mgr;





static int stdio_yuv_yv12_read(nv_encoder_struct*pencoder,void*data,size_t width,size_t height,size_t fstride,size_t bstride)
{

    int nr=0;
    int n=0;
    ENCODER*enc=pencoder->dummy[0];

    stdio_mgr*mgr=nv_encoder_get_io_data(pencoder);

    int pos=0;
    int skip=fstride-width;
//    nr=fread(data,1,stride*height*3/2,enc->ifp);

    int h;
    for(h=0;h<height;h++){
        n=fread(data+pos,1,width,mgr->ifp);
        if(n<0)
            return -1;
        pos+=bstride;
        nr+=n;
        fseek(mgr->ifp,skip,SEEK_CUR);
    }

    for(h=0;h<(height>>1);h++){
        n=fread(data+pos,1,width>>1,mgr->ifp);
        if(n<0)
            return -1;
        pos+=(bstride>>1);
        nr+=n;
        fseek(mgr->ifp,skip>>1,SEEK_CUR);
    }

    for(h=0;h<(height>>1);h++){
        n=fread(data+pos,1,width>>1,mgr->ifp);
        if(n<0)
            return -1;
        pos+=(bstride>>1);
        nr+=n;
        fseek(mgr->ifp,skip>>1,SEEK_CUR);
    }

    return nr;
}



static int stdio_write(nv_encoder_struct*pencoder,void*data,size_t framesize)
{
    int nw;
    stdio_mgr*mgr=nv_encoder_get_io_data(pencoder);
//    fprintf(stderr,"Frame size:%d\n",framesize);
    nw=fwrite(data,1,framesize,mgr->ofp);
//    fprintf(stderr,"Write [%d]\n",nw);
    return nw;

}



int nv_encoder_set_std_io(nv_encoder_struct*pencoder,FILE*src,FILE*dst)
{

    ENCODER*enc=(ENCODER*)pencoder->dummy[0];

    stdio_mgr*mgr=calloc(1,sizeof(stdio_mgr));
    mgr->ifp=src;
    mgr->ofp=dst;

    enc->st_reader=stdio_yuv_yv12_read;
    enc->st_writer=stdio_write;

    return 0;
}



int nv_encoder_set_read(nv_encoder_struct*pencoder,nv_encoder_read_data read,void*iodata)
{
    if(!pencoder)
        return;

    ENCODER*enc=(ENCODER*)pencoder->dummy[0];

    enc->io_data=iodata;
    enc->st_reader=read;

    return 0;

}


int nv_encoder_set_write(nv_encoder_struct*pencoder,nv_encoder_write_data write,void*iodata)
{
    if(!pencoder)
        return;

    ENCODER*enc=(ENCODER*)pencoder->dummy[0];

    enc->io_data=iodata;
    enc->st_writer=write;

    return 0;

}








int transfer_stream(enc)
    ENCODER*enc;
{

    NVENCSTATUS nvret;

    if(enc->output.hBitstreamBuffer==NULL && enc->output.bEOSFlag == FALSE){
        //ERROR
        fprintf(stderr,"Error::Copy Bitstream Data\n");
    }

    if(enc->output.bEOSFlag)
        return 0;//End of stream


    NV_ENC_LOCK_BITSTREAM bstream={0,};
//    bzero(&bstream,sizeof(NV_ENC_LOCK_BITSTREAM));

    bstream.version=NV_ENC_LOCK_BITSTREAM_VER;

    bstream.outputBitstream=enc->output.hBitstreamBuffer;
    bstream.doNotWait=0;//FALSE;//blocking when output not ready..


    nvret=enc->encapi->nvEncLockBitstream(enc->handler,&bstream);
    //<CHK>
    CHK_NVENC_ERR(nvret);
    //if ok
//    fprintf(stderr,"Write bitstream: %d\n",bstream.bitstreamSizeInBytes) ;
    int nw;
    if(enc->st_writer)
        nw=enc->st_writer(enc->caller,bstream.bitstreamBufferPtr,bstream.bitstreamSizeInBytes);
    else{
        fprintf(stderr,"WARN::No Write Callback..\n");
        nw=0;
    }

 ////
    nvret=enc->encapi->nvEncUnlockBitstream(enc->handler,enc->output.hBitstreamBuffer);
    CHK_NVENC_ERR(nvret);


    return 1;//OK
}



void*thr_worker(void*arg)
{
    

    ENCODER*enc=(ENCODER*)arg;
    int ret;
    while(1){


        pthread_mutex_lock(&enc->mutex);
        while(!enc->b_encode)
            pthread_cond_wait(&enc->cond,&enc->mutex);

        ret=transfer_stream(enc);
        if(!ret){
            fprintf(stderr,"End of Stream..\n");
            break;
        }
//        fprintf(stderr,".\r");

        enc->b_encode=0;

        pthread_cond_signal(&enc->cond);
        pthread_mutex_unlock(&enc->mutex);

    }
    
    pthread_mutex_unlock(&enc->mutex);

    return NULL;
}






static const GUID fake_client_guid={0,};

int open_encoder_session(enc)
    ENCODER* enc;
{

    int ret=0;
    NVENCSTATUS nvret;
    
    //Init Cuda
    //

    CUresult cuResult;
    CUdevice cuDevice;
    CUcontext cuContext;


    if(enc->config->iface_type==NV_ENC_IFACE_CUDA){


        cuResult=cuInit(0);
        CHK_CUDA_ERR(cuResult);

        int devcount;
    
        cuResult=cuDeviceGetCount(&devcount);
        CHK_CUDA_ERR(cuResult);
    
        printf("Dev Count:= %d\n",devcount);
    
        cuResult=cuDeviceGet(&cuDevice,enc->config->device_id);
        CHK_CUDA_ERR(cuResult);
    
        cuResult=cuCtxCreate(&cuContext,0,cuDevice);
        CHK_CUDA_ERR(cuResult);
    
    
        enc->st_enc_session_par.device = cuContext;//reinterpret_cast<void *>(m_cuContext);
        enc->st_enc_session_par.deviceType = NV_ENC_DEVICE_TYPE_CUDA;

    }else{
        fprintf(stderr,"Error::Encoder interface not supported![Only CUDA supported]\n");
        return -1;
    }


    GUID clientKey=fake_client_guid;

    enc->st_enc_session_par.version=NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
    enc->st_enc_session_par.clientKeyPtr=&clientKey;
    enc->st_enc_session_par.apiVersion= NVENCAPI_VERSION;



    nvret=enc->encapi->nvEncOpenEncodeSessionEx(&enc->st_enc_session_par,&enc->handler);
    CHK_NVENC_ERR(nvret);


    return ret;

}


int set_guid(enc)
    ENCODER*enc;
{

    int ret;
    NVENCSTATUS nvret;


    int i;

    nvret=enc->encapi->nvEncGetEncodeGUIDCount(enc->handler,&enc->nb_encode_guid);
    //<CHK>
    CHK_NVENC_ERR(nvret);


    GUID *encode_guids=calloc(enc->nb_encode_guid,sizeof(GUID));
    unsigned int n_encode_guid=0;

    nvret=enc->encapi->nvEncGetEncodeGUIDs(enc->handler,encode_guids,enc->nb_encode_guid,&n_encode_guid);
    //<CHK>
    CHK_NVENC_ERR(nvret);

    if(n_encode_guid!= enc->nb_encode_guid){
        //Never get here        
        fprintf(stderr,"????? No. of Encode_GUID\n");
    }


    for(i=0;i<n_encode_guid;i++){
        if(compareGUIDs(encode_guids[i],NV_ENC_CODEC_H264_GUID)){
            break;
        }
    }

    fprintf(stderr,"-> Encode Guid[H264(%d)]\n",i);

    enc->st_encode_guid=encode_guids[i];

    free(encode_guids);


//Preset GUIDs;
    nvret=enc->encapi->nvEncGetEncodePresetCount(enc->handler,enc->st_encode_guid,&enc->nb_preset_guid);
    //<CHK>
    CHK_NVENC_ERR(nvret);

    fprintf(stderr,"INFO::Preset GUID Count:%d\n",enc->nb_preset_guid);

    GUID *preset_guids=calloc(enc->nb_preset_guid,sizeof(GUID));
    unsigned int n_preset_guid=0;

    nvret=enc->encapi->nvEncGetEncodePresetGUIDs(enc->handler,enc->st_encode_guid,preset_guids,enc->nb_preset_guid,&n_preset_guid);
    //<CHK>
    CHK_NVENC_ERR(nvret);
    
    if(n_preset_guid!= enc->nb_preset_guid){
        fprintf(stderr,"????? No. of Preset_GUID\n");
    }

    GUID guid;

#if 1

    for(i=0;i<n_preset_guid;i++){
        guid=get_preset_by_index(i);
        if(compareGUIDs(preset_guids[i],guid)){
            fprintf(stderr,"Find preset:%d \n",i);
            break;
        }
    }
    if(i==n_preset_guid){
        fprintf(stderr,"WARN:: Preset GUID not found,using 0 default \n");
        i=0;
    }else{
        fprintf(stderr,"-> Preset Guid[%d]\n",i);

    }

    enc->st_preset_guid=preset_guids[i];

#else

    for(i=0;i<n_preset_guid;i++){
        if(i==enc->config->preset){
//        if(compareGUIDs(preset_guids[i],NV_ENC_PRESET_LOW_LATENCY_DEFAULT_GUID))
//            fprintf(stderr,"Find preset [NV_ENC_PRESET_LOW_LATENCY_DEFAULT_GUID]\n");
            fprintf(stderr,"Find preset:%d \n",i);
            break;
        }
    }
    if(i==n_preset_guid){
            fprintf(stderr,"WARN:: Preset GUID not found,using 0 default \n");
            i=0;
    }
    fprintf(stderr,"-> Preset Guid[%d]\n",i);

    enc->st_preset_guid=preset_guids[i];
#endif


    enc->st_encode_config.version=NV_ENC_CONFIG_VER;
    enc->st_preset_config.version=NV_ENC_PRESET_CONFIG_VER;
    enc->st_preset_config.presetCfg.version=NV_ENC_CONFIG_VER;


    nvret=enc->encapi->nvEncGetEncodePresetConfig(enc->handler,enc->st_encode_guid,enc->st_preset_guid,&enc->st_preset_config);
    //<CHK>
    CHK_NVENC_ERR(nvret);

    free(preset_guids);

    memcpy(&enc->st_encode_config,&enc->st_preset_config.presetCfg,sizeof(NV_ENC_CONFIG));

//Profile GUIDs;
//
    nvret=enc->encapi->nvEncGetEncodeProfileGUIDCount(enc->handler,enc->st_encode_guid,&enc->nb_profile_guid);
    //<CHK>
    CHK_NVENC_ERR(nvret);
    //
    fprintf(stderr,"-> %d profile GUIDs found\n",enc->nb_profile_guid);

    GUID *profile_guids=calloc(enc->nb_profile_guid,sizeof(GUID));
    unsigned int n_profile_guid=0;

    nvret=enc->encapi->nvEncGetEncodeProfileGUIDs(enc->handler,enc->st_encode_guid,profile_guids,enc->nb_profile_guid,&n_profile_guid);
    //<CHK>
    CHK_NVENC_ERR(nvret);

    if(n_profile_guid!= enc->nb_profile_guid){
        //Never enter here 
        fprintf(stderr,"????? No. of Profile_GUID\n");
    }

#if 1

    for(i=0;i<n_profile_guid;i++){
        guid=get_preset_by_index(i);
        if(compareGUIDs(profile_guids[i],guid)){
            fprintf(stderr,"Find Profile:%d \n",i);
            break;
        }
    }
    if(i==n_profile_guid){
        fprintf(stderr,"WARN:: Profile GUID not found,using 0 default \n");
        i=0;
    }else{
        fprintf(stderr,"-> Profile Guid[%d]\n",i);

    }

    enc->st_profile_guid=profile_guids[i];


#else

    //FIXME
    //using enc->config->profile to determine st_profile_guid
    enc->st_profile_guid=NV_ENC_CODEC_PROFILE_AUTOSELECT_GUID;

#endif

    free(profile_guids);


    return ret;

}





int set_format(enc)
    ENCODER*enc;
{
    
    int ret=0;
    NVENCSTATUS nvret;

    nvret=enc->encapi->nvEncGetInputFormatCount(enc->handler,enc->st_encode_guid,&enc->nb_input_fmt);
    //<CHK>
    CHK_NVENC_ERR(nvret);

    NV_ENC_BUFFER_FORMAT*input_fmts=calloc(enc->nb_input_fmt,sizeof(NV_ENC_BUFFER_FORMAT));
    unsigned int n_input_fmt;

    nvret=enc->encapi->nvEncGetInputFormats(enc->handler,enc->st_encode_guid,input_fmts,enc->nb_input_fmt,&n_input_fmt);
    //<CHK>
    CHK_NVENC_ERR(nvret);


    fprintf(stderr,"-> %d InputFormats found.\n",enc->nb_input_fmt);

    if(n_input_fmt!= enc->nb_input_fmt){
        //Never get here        
        fprintf(stderr,"????? No. of Input Formats\n");
    }

    fprintf(stderr,"Number of Supported Format(%d)\n",n_input_fmt);

//====
//====
    enc->st_input_fmt=enc->config->input_fmt;


    free(input_fmts);

    return ret;
}





void init_encoder(enc)
    ENCODER*enc;
{
    
     
    int ret;
    NVENCSTATUS nvret;


    //Query Caps..

    
#if 1
    int val;

    NV_ENC_CAPS_PARAM cap_pars;
    cap_pars.version=NV_ENC_CAPS_PARAM_VER;
    cap_pars.capsToQuery=NV_ENC_CAPS_ASYNC_ENCODE_SUPPORT;

    nvret=enc->encapi->nvEncGetEncodeCaps(enc->handler,enc->st_encode_guid,
            &cap_pars,&val);
    CHK_NVENC_ERR(nvret);
    printf(" Async Mode Caps[%d]\n",val);


    cap_pars.capsToQuery=NV_ENC_CAPS_SUPPORT_FIELD_ENCODING;
    nvret=enc->encapi->nvEncGetEncodeCaps(enc->handler,enc->st_encode_guid,
            &cap_pars,&val);
    CHK_NVENC_ERR(nvret);
    printf(" Support FieldEncoding Caps[%d]\n",val);





#endif


/**
 * Fill enc->st_init_enc_pars;
 *
 */

    enc->st_init_enc_pars.version=NV_ENC_INITIALIZE_PARAMS_VER;

    enc->st_encode_config.version=NV_ENC_CONFIG_VER;



    enc->st_frame_width=enc->config->width;
    enc->st_frame_height=enc->config->height;

    enc->st_max_width=enc->config->maxWidth;
    enc->st_max_height=enc->config->maxHeight;


    enc->st_init_enc_pars.darHeight=enc->config->height;//Aspect ratio
    enc->st_init_enc_pars.darWidth=enc->config->width;
    enc->st_init_enc_pars.encodeHeight=enc->config->height;
    enc->st_init_enc_pars.encodeWidth=enc->config->width;

    enc->st_init_enc_pars.maxEncodeHeight=enc->config->maxHeight;
    enc->st_init_enc_pars.maxEncodeWidth=enc->config->maxWidth;

    enc->st_init_enc_pars.frameRateNum=enc->config->frameRateNum;
    enc->st_init_enc_pars.frameRateDen=enc->config->frameRateDen;

    enc->st_init_enc_pars.enableEncodeAsync=enc->b_async_mode;

    enc->st_init_enc_pars.enablePTD=enc->config->enablePTD;

    enc->st_init_enc_pars.encodeGUID=enc->st_encode_guid;
    enc->st_init_enc_pars.presetGUID=enc->st_preset_guid;


    enc->st_init_enc_pars.encodeConfig=&enc->st_encode_config;
//    enc->st_init_enc_pars.encodeConfig->version=NV_ENC_CONFIG_VER;

    enc->st_init_enc_pars.encodeConfig->monoChromeEncoding=0;
    enc->st_init_enc_pars.encodeConfig->encodeCodecConfig.h264Config.disableDeblockingFilterIDC=0;
    enc->st_init_enc_pars.encodeConfig->encodeCodecConfig.h264Config.disableSPSPPS=0;
/*
 *
 *preset was set
 */

#if 0
    enc->st_init_enc_pars.encodeConfig->encodeCodecConfig.h264Config.chromaFormatIDC=enc->config->chromaFormatIDC;
//   enc->st_init_enc_pars.encodeConfig->encodeCodecConfig.h264Config.chromaFormatIDC=enc->config->chromaFormatIDC;

    enc->st_init_enc_pars.encodeConfig->frameFieldMode=(enc->config.fieldEncoding>0)? NV_ENC_PARAMS_FRAME_FIELD_MODE_FIELD:NV_ENC_PARAMS_FRAME_FIELD_MODE_FRAME;

    enc->st_init_enc_pars.encodeConfig->rcParams.rateControlMode=enc->st_init_enc_pars.encodeConfig->rcPrams.rateControl;
    enc->st_init_enc_pars.encodeConfig->rcParams.constQP.qpIntra=enc->st_init_enc_pars.encodeConfig->rcPrams.constQP.qpIntra;
    enc->st_init_enc_pars.encodeConfig->rcParams.constQP.qpInterP=enc->st_init_enc_pars.encodeConfig->rcPrams.constQP.qpIntraP;
    enc->st_init_enc_pars.encodeConfig->rcParams.constQP.qpInterB=enc->st_init_enc_pars.encodeConfig->rcPrams.constQP.qpIntraB;
    enc->st_init_enc_pars.encodeConfig->profileGUID=enc->st_init_enc_pars.encodeConfig->profileGUID;

    enc->st_init_enc_pars.encodeConfig->frameIntervalP=enc->st_init_enc_pars.encodeConfig->frameIntervalP;
    enc->st_init_enc_pars.encodeConfig->gopLength=enc->st_init_enc_pars.encodeConfig->gopLength;

    enc->st_init_enc_pars.encodeConfig->encodeCodecConfig.h264Config.level=enc->config->level;
    enc->st_init_enc_pars.encodeConfig->encodeCodecConfig.h264Config.sliceMode=H264_SLICE_MODE_NUMSLICES;//
    enc->st_init_enc_pars.encodeConfig->encodeCodecConfig.h264Config.sliceModeData=enc->config->sliceModeData;
    enc->st_init_enc_pars.encodeConfig->encodeCodecConfig.h264Config.separateColourPlaneFlag=enc->config->separateColourPlaneFlag;

    enc->st_init_enc_pars.encodeConfig->encodeCodecConfig.h264Config.idrPeriod=enc->config->idr_period;


    enc->st_init_enc_pars.encodeConfig->mvPrecision=enc->config->mvPrecision;
    enc->st_init_enc_pars.encodeConfig->frameFieldMode=(enc->config->fieldEncoding > 0)? NV_ENC_PARAMS_FRAME_FIELD_MODE_FIELD : NV_ENC_PARAMS_FRAME_FIELD_MODE_FRAME;
    enc->st_init_enc_pars.encodeConfig->encodeCodecConfig.h264Config.bdirectMode=enc->config->numBFrames > 0 ? NV_ENC_H264_BDIRECT_MODE_TEMPORAL : NV_ENC_H264_BDIRECT_MODE_DISABLE;
    enc->st_init_enc_pars.encodeConfig->encodeCodecConfig.h264Config.disableSPSPPS=(NV_ENC_H264_ENTROPY_CODING_MODE)!!enc->config->vle_cabac_enable;

    enc->st_init_enc_pars.encodeConfig->encodeCodecConfig.h264Config.entropyCodingMode=NV_ENC_H264_ENTROPY_CODING_MODE_AUTOSELECT;







#else
#endif
    ///////
    //
    enc->st_init_enc_pars.encodeConfig->encodeCodecConfig.h264Config.chromaFormatIDC=enc->config->chromaFormatIDC;


    enc->st_init_enc_pars.encodeConfig->rcParams.rateControlMode=enc->config->rateControl;
    enc->st_init_enc_pars.encodeConfig->rcParams.constQP.qpIntra=enc->config->initialRCQP.qpIntra;
    enc->st_init_enc_pars.encodeConfig->rcParams.constQP.qpInterP=enc->config->initialRCQP.qpInterP;
    enc->st_init_enc_pars.encodeConfig->rcParams.constQP.qpInterB=enc->config->initialRCQP.qpInterB;
    enc->st_init_enc_pars.encodeConfig->profileGUID=enc->st_profile_guid;

    enc->st_init_enc_pars.encodeConfig->frameIntervalP=enc->config->numBFrames+1;
    enc->st_init_enc_pars.encodeConfig->gopLength=enc->config->gopLength;

    enc->st_init_enc_pars.encodeConfig->encodeCodecConfig.h264Config.level=enc->config->level;
    enc->st_init_enc_pars.encodeConfig->encodeCodecConfig.h264Config.sliceMode=enc->config->sliceMode;//H264_SLICE_MODE_NUMSLICES;//
    enc->st_init_enc_pars.encodeConfig->encodeCodecConfig.h264Config.sliceModeData=enc->config->sliceModeData;
    enc->st_init_enc_pars.encodeConfig->encodeCodecConfig.h264Config.separateColourPlaneFlag=enc->config->separateColourPlaneFlag;

    enc->st_init_enc_pars.encodeConfig->encodeCodecConfig.h264Config.idrPeriod=enc->config->idr_period;


    enc->st_init_enc_pars.encodeConfig->mvPrecision=enc->config->mvPrecision;
    enc->st_init_enc_pars.encodeConfig->frameFieldMode=(enc->config->fieldEncoding > 0)? NV_ENC_PARAMS_FRAME_FIELD_MODE_FIELD : NV_ENC_PARAMS_FRAME_FIELD_MODE_FRAME;
    enc->st_init_enc_pars.encodeConfig->encodeCodecConfig.h264Config.bdirectMode=enc->config->numBFrames > 0 ? NV_ENC_H264_BDIRECT_MODE_TEMPORAL : NV_ENC_H264_BDIRECT_MODE_DISABLE;
    enc->st_init_enc_pars.encodeConfig->encodeCodecConfig.h264Config.disableSPSPPS=(NV_ENC_H264_ENTROPY_CODING_MODE)!!enc->config->vle_cabac_enable;

    enc->st_init_enc_pars.encodeConfig->encodeCodecConfig.h264Config.entropyCodingMode=NV_ENC_H264_ENTROPY_CODING_MODE_AUTOSELECT;

//
//

    enc->st_init_enc_pars.encodeConfig->rcParams.averageBitRate=enc->config->avgBitRate;
    enc->st_init_enc_pars.encodeConfig->rcParams.maxBitRate=enc->config->peakBitRate;
    enc->st_init_enc_pars.encodeConfig->rcParams.enableInitialRCQP=enc->config->enableInitialRCQP;
    enc->st_init_enc_pars.encodeConfig->rcParams.vbvBufferSize=enc->config->vbvBufferSize;
    enc->st_init_enc_pars.encodeConfig->rcParams.vbvInitialDelay=enc->config->vbvInitialDelay;
    enc->st_init_enc_pars.encodeConfig->rcParams.enableAQ=enc->config->enableAQ;

    enc->st_init_enc_pars.encodeConfig->encodeCodecConfig.h264Config.enableIntraRefresh=enc->config->intraRefreshCount>0;
    enc->st_init_enc_pars.encodeConfig->encodeCodecConfig.h264Config.intraRefreshCnt=enc->config->intraRefreshCount;
    enc->st_init_enc_pars.encodeConfig->encodeCodecConfig.h264Config.intraRefreshPeriod=enc->config->intraRefreshPeriod;



    enc->st_encode_config.rcParams.enableInitialRCQP=enc->config->enableInitialRCQP;
    enc->st_encode_config.rcParams.initialRCQP.qpInterB=enc->config->initialRCQP.qpInterB;
    enc->st_encode_config.rcParams.initialRCQP.qpInterP=enc->config->initialRCQP.qpInterP;
    enc->st_encode_config.rcParams.initialRCQP.qpIntra=enc->config->initialRCQP.qpIntra;




    nvret=enc->encapi->nvEncInitializeEncoder(enc->handler,&enc->st_init_enc_pars);
    //<CHK>
    CHK_NVENC_ERR(nvret);
    //



    if(enc->config->idr_period_startup==0)
        enc->st_frame_conf.startup_type=NV_ENC_STARTUP_NORMAL;
    else
        enc->st_frame_conf.startup_type=NV_ENC_STARTUP_SLOW;





    enc->b_encode=0;
    pthread_mutex_init(&enc->mutex,NULL);
    pthread_cond_init(&enc->cond,NULL);



}




void fini_buffers(enc)
    ENCODER*enc;
{
    
    int ret=0;
    NVENCSTATUS nvret;


#ifndef MAPPED_ONLY

    if(enc->config->useMappedResources){
#endif        


        nvret=enc->encapi->nvEncUnmapInputResource(enc->handler,enc->input.hInputSurface);


        nvret=enc->encapi->nvEncUnregisterResource(enc->handler,enc->input.hRegisteredHandle);

        CHK_NVENC_ERR(nvret);

        if(enc->config->iface_type==NV_ENC_IFACE_CUDA){

            cuMemFree((CUdeviceptr)enc->input.pExtAlloc);

            if(enc->config->input_type&NV_ENC_INPUT_HOST_CONV){
                cuMemFreeHost(enc->input.pExtAllocHost);
            }else{//NV_ENC_INPUT_CUDA_CONV
                cuMemFree((CUdeviceptr)enc->input.pExtAllocTmp);
            }

        }

        enc->input.hInputSurface=NULL;


#ifndef MAPPED_ONLY
    }else{
        //NV_ENC_IFACE_NORMAL;

        nvret=enc->encapi->nvEncDestroyInputBuffer(enc->handler,enc->input.hInputSurface);

        CHK_NVENC_ERR(nvret);

        enc->input.hInputSurface=NULL;

    }
#endif

    nvret=enc->encapi->nvEncDestroyBitstreamBuffer(enc->handler,enc->output.hBitstreamBuffer);
    CHK_NVENC_ERR(nvret);

    enc->output.hBitstreamBuffer=NULL;


}





void fini_encoder(enc)
    ENCODER*enc;
{
    
    int ret=0;
    NVENCSTATUS nvret;


    nvret=enc->encapi->nvEncDestroyEncoder(enc->handler);
    CHK_NVENC_ERR(nvret);


    if(enc->config->iface_type==NV_ENC_IFACE_CUDA){
        if(enc->st_cu_ctx){

            CUresult cuResult=CUDA_SUCCESS;
            cuResult=cuCtxDestroy(enc->st_cu_ctx);
            CHK_CUDA_ERR(cuResult);
        }
    }   


    pthread_mutex_destroy(&enc->mutex);
    pthread_cond_destroy(&enc->cond);

    pthread_attr_destroy(&enc->attr);

}






int init_buffers(enc)
    ENCODER*enc;
{

    
    int ret=0;
    NVENCSTATUS nvret;
    CUresult cuResult;


    NV_ENC_REGISTER_RESOURCE reg_res={0,};


    enc->input.dwWidth=enc->st_max_width;
    enc->input.dwHeight=enc->st_max_height;

    //INPUT
    //

#ifndef MAPPED_ONLY
    if(enc->config->useMappedResources){
#endif
        if(enc->config->iface_type==NV_ENC_IFACE_CUDA){

            CUcontext cuCtxCur;
            CUdeviceptr devPtr;

//            cuResult=cuMemAllocPitch(&devPtr,(size_t*)&enc->input.dwCuPitch,enc->st_max_width,enc->st_max_height*3/2,16);
            cuResult=cuMemAlloc(&devPtr,enc->st_max_width*enc->st_max_height*3/2);
            CHK_CUDA_ERR(cuResult);

            enc->input.dwCuPitch=enc->st_max_width;
            //fake pitch 

            fprintf(stderr,"CUDA INPUT BUFFER::Pitch =%u\n",enc->input.dwCuPitch);
            fprintf(stderr,"CUDA INPUT BUFFER::Width =%u\n",enc->st_max_width);
            fprintf(stderr,"CUDA INPUT BUFFER::Height=%u\n",enc->st_max_height);

            enc->input.pExtAlloc=(void*)devPtr;


            if(enc->config->input_type&NV_ENC_INPUT_HOST_CONV){
                cuResult=cuMemAllocHost((void**)&enc->input.pExtAllocHost,enc->input.dwCuPitch*enc->st_max_height*3/2);//YUV420
                CHK_CUDA_ERR(cuResult);
            }else{//NV_ENC_INPUT_CUDA_CONV
                cuResult=cuMemAlloc(&devPtr,enc->st_max_width*enc->st_max_height*4);
//                cuResult=cuMemAllocPitch(&devPtr,(size_t*)&enc->input.dwCuPitch_conv,enc->st_max_width*4,enc->st_max_height,16);
                CHK_CUDA_ERR(cuResult);
                enc->input.pExtAllocTmp=(void*)devPtr;
            }


            enc->input.type=NV_ENC_INPUT_RESOURCE_TYPE_CUDADEVICEPTR;

            enc->input.bufferFmt=enc->st_input_fmt;

        }else{

        //NV_ENC_D3D9;

        }
        //Using Mapped Resource..

        reg_res.version=NV_ENC_REGISTER_RESOURCE_VER;
        reg_res.resourceType=enc->input.type;

        reg_res.resourceToRegister=enc->input.pExtAlloc;
        reg_res.width = enc->input.dwWidth;
        reg_res.height= enc->input.dwHeight;
        reg_res.pitch = enc->input.dwCuPitch;

        nvret=enc->encapi->nvEncRegisterResource(enc->handler,&reg_res);

        CHK_NVENC_ERR(nvret);

        enc->input.hRegisteredHandle=reg_res.registeredResource;

        ///mapres
        NV_ENC_MAP_INPUT_RESOURCE map_res={0,};
        map_res.version=NV_ENC_MAP_INPUT_RESOURCE_VER;
        map_res.registeredResource=enc->input.hRegisteredHandle;
        nvret=enc->encapi->nvEncMapInputResource(enc->handler,&map_res);
        //<CHK>
        CHK_NVENC_ERR(nvret);

        enc->input.hInputSurface=map_res.mappedResource;


#ifndef MAPPED_ONLY
    }else{
       //NV_ENC_IFACE_NORMAL 


        NV_ENC_CREATE_INPUT_BUFFER input_buff={0,};
//        bzero(&input_buff,sizeof(NV_ENC_CREATE_INPUT_BUFFER));
//        input_buff=calloc(1,sizeof(NV_ENC_CREATE_INPUT_BUFFER));
        input_buff.version=NV_ENC_CREATE_INPUT_BUFFER_VER;
        input_buff.width=(enc->st_max_width+31)&~31;
        input_buff.height=(enc->st_max_height+31)&~31;
        input_buff.memoryHeap=NV_ENC_MEMORY_HEAP_SYSMEM_UNCACHED;
        input_buff.bufferFmt=enc->st_input_fmt;
//        input_buff.bufferFmt=enc->config->input_fmt;


        fprintf(stderr,"Width:%d,  Height:%d\n",input_buff.width,input_buff.height);

        nvret=enc->encapi->nvEncCreateInputBuffer(enc->handler,&input_buff);
        //<CHK>
        CHK_NVENC_ERR(nvret);

        enc->input.hInputSurface=input_buff.inputBuffer;
        enc->input.bufferFmt=input_buff.bufferFmt;
        enc->input.dwWidth=(enc->st_max_width+31)&~31;
        enc->input.dwHeight=(enc->st_max_height+31)&~31;


        fprintf(stderr,"CREATE INPUT BUFFER:%p\n",enc->input.hInputSurface);


    }
#endif

    //OUTPUT
    //

    NV_ENC_CREATE_BITSTREAM_BUFFER output_buff;
    bzero(&output_buff,sizeof(NV_ENC_CREATE_BITSTREAM_BUFFER));
    output_buff.version=NV_ENC_CREATE_BITSTREAM_BUFFER_VER;
    
    enc->output.dwSize=1024*1024;

    output_buff.size=enc->output.dwSize;
    output_buff.memoryHeap=NV_ENC_MEMORY_HEAP_AUTOSELECT;

    nvret=enc->encapi->nvEncCreateBitstreamBuffer(enc->handler,&output_buff);
    //<CHK>
    CHK_NVENC_ERR(nvret);
    //

    enc->output.hBitstreamBuffer=output_buff.bitstreamBuffer;
    enc->output.pBitstreamBufferPtr=output_buff.bitstreamBufferPtr;//Reserved and should not be used 


//    fprintf(stderr,"BitStreamBuffer Set..\n");
/////////////////////////////////////

    enc->st_luma_siz=enc->st_max_width*enc->st_max_height;
    enc->st_chroma_siz=(enc->config->chromaFormatIDC==3)?enc->st_luma_siz:enc->st_luma_siz>>2;



    if(enc->config->input_type & NV_ENC_INPUT_CREATE_BUFFER){

//    enc->st_yuv[0]=calloc(1,enc->st_luma_siz+enc->st_chroma_siz*2);//YUV420
        enc->st_yuv[0]=calloc(enc->st_max_height,enc->st_max_width*4);//RGB/YUV

    }else{// NV_ENC_INPUT_USE_SHM
        enc->st_yuv[0]=enc->config->shm_data;

    }

//    enc->st_yuv[0]=0;//

    enc->st_yuv[1]=enc->st_yuv[0]+enc->st_luma_siz;
    enc->st_yuv[2]=enc->st_yuv[1]+enc->st_chroma_siz;

////////////////////////////////////

    enc->st_frame_conf.yuv[0]=enc->st_yuv[0];
    enc->st_frame_conf.yuv[1]=enc->st_yuv[1];
    enc->st_frame_conf.yuv[2]=enc->st_yuv[2];

    enc->st_frame_conf.stride[0]=enc->config->width;
    enc->st_frame_conf.stride[1]=enc->config->width/2;
    enc->st_frame_conf.stride[2]=enc->config->width/2;

    enc->st_frame_conf.width=enc->config->width;
    enc->st_frame_conf.height=enc->config->height;

    //FIXME NV_ENC_CAPS_SUPPORT_FIELD_ENCODING 
    if(enc->config->fieldEncoding ==1){
        enc->st_frame_conf.picStruct=(enc->config->bottomFieldFrist==1)?
            NV_ENC_PIC_STRUCT_FIELD_BOTTOM_TOP:
            NV_ENC_PIC_STRUCT_FIELD_TOP_BOTTOM;

    }else{
        enc->st_frame_conf.picStruct=NV_ENC_PIC_STRUCT_FRAME;
    }


    enc->st_frame_conf.enc_rect=&enc->st_rect;

    enc->st_frame_conf.bReconfigured=0;//TODO


    return ret;
}




void flush_encoder(enc)
    ENCODER*enc;
{

    NVENCSTATUS nvret;

    NV_ENC_PIC_PARAMS pic_pars={0,};

    pic_pars.version=NV_ENC_PIC_PARAMS_VER;

    pic_pars.encodePicFlags=NV_ENC_PIC_FLAG_EOS;
    pic_pars.completionEvent=NULL;//Only sync mode supported

    enc->output.bEOSFlag=1;//Indicate this output is EOS

    nvret=enc->encapi->nvEncEncodePicture(enc->handler,&pic_pars);
    CHK_NVENC_ERR(nvret);

}




static int encode_frame(enc)
    ENCODER*enc;
{
    int ret=0;
    NVENCSTATUS nvret;

    ENC_FRAME_CONFIG*econf=&enc->st_frame_conf;

    int dwWidth=enc->config->width;
    int dwHeight=enc->config->height;
//    int dwSurfWidth=(dwWidth+0x1f)&~0x1f;//align 32 as driver does the same
//    int dwSurfHeight=(dwHeight+0x1f)&~0x1f;

    int dwSurfWidth=enc->input.dwCuPitch;//(dwWidth+0x1f)&~0x1f;//align 32 as driver does the same
    int dwSurfHeight=dwHeight;//(dwHeight+0x1f)&~0x1f;

//    fprintf(stderr,"[[[[[SurfWidth:%d  SurfHeight:%d]]]]]\n",dwSurfWidth,dwSurfHeight);

    unsigned char* input_surf;//luma channel
    unsigned char* input_surf_Ch;//chroma channel
    int lockedPitch;//Buffer Stride..


    //read raw data into buffer
    //

//    nr=read_yuv(enc->ifd,enc->st_yuv,enc->config->width,enc->config->height);

    int nr;

    if(enc->st_reader){
        //FIXME filestride & bufferstride;
//        nr=enc->st_reader(enc->caller,enc->st_yuv[0],enc->config->width,enc->config->height,enc->config->width,enc->config->width);
        nr=enc->st_reader(enc->caller,enc->st_frame_conf.yuv[0],enc->config->width,enc->config->height,enc->config->width,enc->config->width);
        if(nr<=0){
            fprintf(stderr,"READ(%d) <=0\n",nr);
            //Flush Encoder.
            flush_encoder(enc);
            return 0;
        }
    }else{
        
        fprintf(stderr,"WARN:: No read Callback.\n");
        return 0;
    }

    //Synchronous Mode
    //1. load yuv data into buffer

#ifndef MAPPED_ONLY
    if(enc->config->useMappedResources){
#endif        
        if(enc->config->iface_type==NV_ENC_IFACE_CUDA){

            lockedPitch=enc->input.dwCuPitch;
            input_surf=enc->input.pExtAllocHost;
//            input_surf=enc->input.;
            input_surf_Ch=input_surf+(dwSurfHeight*lockedPitch);

        }else{
            //D3D
        }

#ifndef MAPPED_ONLY
    }else{
        //NV_ENC_IFACE_NORMAL        

        NV_ENC_LOCK_INPUT_BUFFER lk_input_buff={0,};
//        bzero(&lk_input_buff,sizeof(NV_ENC_LOCK_INPUT_BUFFER));
        lk_input_buff.version=NV_ENC_LOCK_INPUT_BUFFER_VER;
        lk_input_buff.inputBuffer=enc->input.hInputSurface;

        fprintf(stderr,"INPUT BUFFER:%p\n",enc->input.hInputSurface);

        nvret=enc->encapi->nvEncLockInputBuffer(enc->handler,&lk_input_buff);
        //<CHK>
        CHK_NVENC_ERR(nvret);
        //
        lockedPitch=lk_input_buff.pitch;
        input_surf=lk_input_buff.bufferDataPtr;
        input_surf_Ch=input_surf+(dwSurfHeight*lockedPitch);

    }
#endif


    enc->input.inputType=enc->config->input_type;
    
//    enc->input.bufferFmt == NV_ENC_BUFFER_FORMAT_NV12_PL;

//=====
//=====
//

    if(enc->input.inputType & NV_ENC_INPUT_BGRX){


        int af_luma_siz=dwSurfWidth*dwHeight;
        int af_chroma_siz=af_luma_siz/4;

        /////
        input_surf=enc->input.pExtAllocHost;

//        fprintf(stderr,"x:%d,y:%d,w:%d,h:%d\n",econf->enc_rect->x,econf->enc_rect->y,econf->enc_rect->w,econf->enc_rect->h);

        if(econf->enc_rect->w==0||econf->enc_rect->h==0){
            //Need not load data anymore
            goto LOADDATEOK;
        }

        if(enc->input.inputType&NV_ENC_INPUT_CUDA_CONV){

            load_rgb_bgrx_cuda((unsigned char*)enc->input.pExtAlloc,(unsigned char*)enc->input.pExtAllocTmp,
                econf->yuv[0],//
                econf->enc_rect->x,econf->enc_rect->y,econf->enc_rect->w,econf->enc_rect->h,
                econf->width,econf->height,lockedPitch);

        }else{
//            load_rgb_bgrx(input_surf,econf->yuv[0],econf->enc_rect->x,econf->enc_rect->y,econf->enc_rect->w,econf->enc_rect->h,econf->height,lockedPitch);
//          FIXME:: prefer Intel ipp conversion utils..
            load_rgb_bgrx_2(input_surf,econf->yuv[0],econf->enc_rect->x,econf->enc_rect->y,econf->enc_rect->w,econf->enc_rect->h,econf->height,lockedPitch);

//            load_rgb_bgrx_(econf->yuv[0],input_surf,econf->enc_rect->x,econf->enc_rect->y,econf->enc_rect->w,econf->enc_rect->h,econf->width,econf->height,0,0);
        }
//
    }else if(enc->input.inputType & NV_ENC_INPUT_YV12){


        int af_luma_siz=dwSurfWidth*dwHeight;
        int af_chroma_siz=af_luma_siz/4;

        /////
        input_surf=enc->input.pExtAllocHost;


        unsigned char*YUV[3];
        YUV[0]=input_surf;
//        YUV[1]=input_surf+af_luma_siz;
//        YUV[2]=input_surf+af_luma_siz+af_chroma_siz;

        if(enc->input.inputType&NV_ENC_INPUT_CUDA_CONV){
            //load_yuv_yv12_cuda();

        }else{
            load_yuv_yv12(econf->yuv[0],YUV[0],econf->width,econf->height,econf->width,lockedPitch);

        }

    }else if(enc->input.inputType & NV_ENC_INPUT_NV12){
    
        int af_luma_siz=dwSurfWidth*dwHeight;
        int af_chroma_siz=af_luma_siz/2;

        /////
        input_surf=enc->input.pExtAllocHost;

        unsigned char*YUV[3];
        YUV[0]=input_surf;
        YUV[1]=input_surf+af_luma_siz;

        load_yuv_nv12(econf->yuv[0],YUV[0],econf->width,econf->height,econf->width,lockedPitch);
        //FIXME Direct copy yv12 to Device buffer. 
    
    }else{
        fprintf(stderr,"Only YV12/NV12/RGBX Supported..\n");
        return -1;
    }

LOADDATEOK:


#ifndef MAPPED_ONLY
    if(enc->config->useMappedResources){
#endif         

        if(enc->config->iface_type==NV_ENC_IFACE_CUDA){

            CUcontext cuCtxCur;
            CUresult cuResult;

            if(enc->input.inputType&NV_ENC_INPUT_HOST_CONV){
                cuResult=
                cuMemcpyHtoD((CUdeviceptr)enc->input.pExtAlloc,enc->input.pExtAllocHost,
                        enc->input.dwCuPitch*enc->input.dwHeight*3/2);

//                cuMemcpyHtoDAsync((CUdeviceptr)enc->input.pExtAlloc,enc->input.pExtAllocHost,
//                        enc->input.dwCuPitch*enc->input.dwHeight*3/2,NULL);

            }

        }else{
            //WINDOWS...specified.

        }
        


#ifndef MAPPED_ONLY

    }else{
        
        nvret=enc->encapi->nvEncUnlockInputBuffer(enc->handler,enc->input.hInputSurface);
        CHK_NVENC_ERR(nvret);

    }
#endif
 

    //2. encode a frame


    NV_ENC_PIC_PARAMS pic_pars;
    bzero(&pic_pars,sizeof(NV_ENC_PIC_PARAMS));

    pic_pars.version=NV_ENC_PIC_PARAMS_VER;

    pic_pars.inputBuffer=enc->input.hInputSurface;
    pic_pars.bufferFmt=enc->input.bufferFmt;
    pic_pars.inputWidth=enc->input.dwWidth;
    pic_pars.inputHeight=enc->input.dwHeight;
    pic_pars.outputBitstream=enc->output.hBitstreamBuffer;
    pic_pars.completionEvent=NULL;//Only Synchronous mode supported
    pic_pars.pictureStruct=econf->picStruct;
    pic_pars.encodePicFlags =0;
    pic_pars.inputTimeStamp =0;
    pic_pars.inputDuration  =0;
    pic_pars.codecPicParams.h264PicParams.sliceMode =enc->st_encode_config.encodeCodecConfig.h264Config.sliceMode;
    pic_pars.codecPicParams.h264PicParams.sliceModeData =enc->st_encode_config.encodeCodecConfig.h264Config.sliceModeData;

#if 1
    if(econf->startup_type==NV_ENC_STARTUP_SLOW){
        if(econf->startup_count<=enc->config->idr_period && econf->startup_count%enc->config->idr_period_startup==0){
//            pic_pars.codecPicParams.h264PicParams.refPicFlag=1;
            pic_pars.encodePicFlags|=NV_ENC_PIC_FLAG_FORCEIDR;//only valid when enablePTD==1
//            pic_pars.encodePicFlags|=NV_ENC_PIC_FLAG_FORCEINTRA;
        }
    }
#else
    pic_pars.codecPicParams.h264PicParams.forceIntraRefreshWithFrameCnt=3;
#endif

    econf->startup_count++;

    memcpy(&pic_pars.rcParams,&enc->st_encode_config.rcParams,sizeof(NV_ENC_RC_PARAMS));

    ////?????????TODO ?remove?
/*
    if(!enc->st_init_enc_pars.enablePTD){
        pic_pars.codecPicParams.h264PicParams.refPicFlag = 1;
        pic_pars.codecPicParams.h264PicParams.displayPOCSyntax=2*enc->dwframe_num_in_gop;
        pic_pars.pictureType=(enc->dwframe_num_in_gop%enc->config->gopLength)==0 ? NV_ENC_PIC_TYPE_IDR:NV_ENC_PIC_TYPE_P;

    }
*/
    if(enc->b_async_mode==FALSE &&
            enc->st_init_enc_pars.enablePTD==1){

        enc->output.bWaitOnEvent=FALSE;

    }

    pthread_mutex_lock(&enc->mutex);
    while(enc->b_encode)
        pthread_cond_wait(&enc->cond,&enc->mutex);

    nvret=enc->encapi->nvEncEncodePicture(enc->handler,&pic_pars);
    //<CHK>
    CHK_NVENC_ERR(nvret);

//    fprintf(stderr,":");
    
    enc->b_encode=1;

    enc->st_current_frame++;

    pthread_cond_signal(&enc->cond);
    pthread_mutex_unlock(&enc->mutex);


 //   enc->dwframe_num_in_gop++;
    ret=nr;
    return ret;

}





int nv_encoder_encode(nv_encoder_struct*pencoder,int eos)
{
    
    int ret=0;
    ENCODER*enc=(ENCODER*)pencoder->dummy[0];


    if(!eos && (enc->st_current_frame<=enc->config->num_frames || enc->config->num_frames==0) ){
//        fprintf(stderr,"Encode....\n");
        ret=encode_frame(enc);

    }else{
        fprintf(stderr,"Flush....\n");
        flush_encoder(enc);
    }

    return ret;

}








void fini_encoder_config(conf)
    ENC_CONFIG*conf;
{
    if(conf)
        free(conf);
}



ENC_CONFIG*init_encoder_config()
{
    ENC_CONFIG*conf=calloc(1,sizeof(ENC_CONFIG));
    if(!conf){
        return NULL;
    }
    _init_enc_config(conf);


    return conf;
}





void nv_encoder_get_default_config(nv_encoder_config*pconfig)
{

    bzero(pconfig,sizeof(nv_encoder_config));

    pconfig->device_id=0;

    pconfig->width=1280;
    pconfig->height=720;

    pconfig->frame_rate_num=25;
    pconfig->frame_rate_den=1;

    pconfig->gop_length=25;

    pconfig->conv_type=0;//NV_ENC_INPUT_CUDA_CONV;
    pconfig->input_type=3;//NV_ENC_INPUT_BGRX|NV_ENC_INPUT_CREATE_BUFFER;//rgbx
    
////

    pconfig->rate_control=1;
    pconfig->avg_bit_rate=3000000;
    pconfig->peak_bit_rate =3000000;

//    pconfig->field_encoding=0;

    pconfig->num_frames=0;

}








static void fini_nv_encoder(ENCODER*enc)
{

    if(!enc)
        return;

    fini_buffers(enc);

    fini_encoder(enc);


    fini_encoder_config(enc->config);

    fini_encode_api(enc->encapi);

    if(enc->config->input_type & NV_ENC_INPUT_CREATE_BUFFER){
        free(enc->st_yuv[0]);
    }
    free(enc);

}


static ENCODER* init_nv_encoder(ENC_CONFIG*conf)
{

    int ret;

    ENCODER*enc=calloc(1,sizeof(ENCODER));
    if(!enc){
        return NULL;
    }

    enc->b_async_mode=0;


    enc->config=conf;

    enc->encapi=init_encode_api();
    if(!enc->encapi){
        return NULL;
    }


    open_encoder_session(enc);
    fprintf(stderr,"After Open Encode Session..\n");

    set_guid(enc);
    fprintf(stderr,"After Set GUIDs..\n");
    
    set_format(enc);
    fprintf(stderr,"After Init Formats..\n");

    init_encoder(enc);
    fprintf(stderr,"After Init Encoder..\n");

    init_buffers(enc);
    fprintf(stderr,"After Init Resource..\n");


    pthread_attr_setdetachstate(&enc->attr,PTHREAD_CREATE_DETACHED);

    pthread_create(&enc->thrid,&enc->attr,thr_worker,enc);


    ret=clock_gettime(CLOCK_REALTIME,&enc->ts0);
    if(ret<0){
        fprintf(stderr,"WARN:: GET TIME...\n");
    }

    return enc;
}





int nv_encoder_init(nv_encoder_struct*pencoder ,nv_encoder_config*pconfig)
{

    int ret;

#ifdef _DEBUG_

/*  Register Signal
*/

    struct sigaction sa;
    bzero(&sa,sizeof(sa));
    sa.sa_handler=sig_handler;

    ret=sigaction(SIGINT,&sa,NULL);
    if(ret<0){
        fprintf(stderr,"WARN:: Regidter Signal Handler..\n");
    }

#endif

    ENC_CONFIG* conf=init_encoder_config();
    if(!conf){
        fprintf(stderr,"ERROR:: Can not allocate configuration struct..\n");
        return -1;
    }

    //read configuration from pconfig;
    merge_config(conf,pconfig);


    ENCODER*enc=init_nv_encoder(conf);
    enc->caller=pencoder;

#ifdef _DEBUG_
    global_enc_ref=enc;
#endif

    pencoder->dummy[0]=enc;
    pencoder->dummy[1]=pconfig;

    return 0;

}



int nv_encoder_fini(nv_encoder_struct*pencoder)
{
    
    ENCODER*enc=(ENCODER*)pencoder->dummy[0];

    fini_nv_encoder(enc);

    fprintf(stderr,"Encoder Finalized...\n");

}





#ifdef _DEBUG_

/*for Debug*/
void sig_handler(int signo)
{
    
    struct timespec ts;
    int ret;

    if(signo==SIGINT){

        ret=clock_gettime(CLOCK_REALTIME,&global_enc_ref->ts1);
        if(ret<0){
            fprintf(stderr,"WARN:: GET TIME...\n");
        }

        if(global_enc_ref->ts1.tv_nsec<global_enc_ref->ts0.tv_nsec){
            global_enc_ref->ts1.tv_sec-=1;
            ts.tv_nsec=global_enc_ref->ts1.tv_nsec+1000000000-global_enc_ref->ts0.tv_nsec;
        }else{
            ts.tv_nsec=global_enc_ref->ts1.tv_nsec-global_enc_ref->ts0.tv_nsec;
        }

        ts.tv_sec=global_enc_ref->ts1.tv_sec-global_enc_ref->ts0.tv_sec;
    }


    //milliseconds
    long long tval=ts.tv_sec*1000+ts.tv_nsec/1000000;



    fprintf(stderr,"\033[33m[TotalMillisecs:%lld]{Frames:%d}\033[0m\n",tval,global_enc_ref->st_current_frame);
    fprintf(stderr,"\033[32mProduce 1 Frame take %lld milliseconds...\033[0m\n",tval/global_enc_ref->st_current_frame);

    kill(0,SIGTERM);

}




#endif





