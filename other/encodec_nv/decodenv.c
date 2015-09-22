#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<string.h>

#include<unistd.h>
#include<fcntl.h>
#include<sys/types.h>
#include<cuda.h>
#include<cuviddec.h>

#include<nvcuvid.h>

#include"drvapi_error_string.h"
#include"chks.h"







/*
void _nv12_to_yv12(char*from,char*to,int width,int height,int istride,int ostride)
{


    char* nv12_luma=from;
    char* yv12_luma=to;

    char* nv12_chroma=nv12_luma+istride*height;
    char* yv12_cv=yv12_luma+ostride*height;
    char* yv12_cu=yv12_luma+ostride*height+ostride*height/4;


    int l;
    for(l=0;l<height;l++){
    //copy luma channel
        memcpy(yv12_luma+l*ostride,nv12_luma+l*istride,width);
            
    }

    int pos=0;
    int l;
    for(l=0;l<height/2;l++){
    //copy luma channel
        char*yv12_pc=yv12_cv+l*;

    }

}

*/










#define MEM_PAD(x,mask)((x+mask)&~mask)



struct _DEC_CONFIG{

    int width;
    int height;

    int maxsurfaces;
    cudaVideoCodec decode_type;

    char*filename;

};
typedef struct _DEC_CONFIG DEC_CONFIG;






typedef struct _VID_SOURCE{

    CUvideosource vid_source;
    void* p_userdata; 
    CUVIDEOFORMAT vid_format;
//    que_t*frameQ;
//    DECODER*p_dec;
}DEC_SOURCE;


typedef struct _VID_PARSER{

    CUvideoparser vid_parser;
    CUcontext* p_ctx;
    void*p_userdata;
//    que_t*frameQ;
//    DECODER*p_dec;

}DEC_PARSER;









struct _DECODER_MAIN{


    DEC_CONFIG*config;

    CUvideodecoder decoder;
//    CUVIDDECODECREATEINFO info;

    cudaVideoCreateFlags create_flags;

    int n_device;
    CUdevice dec_dev;
    CUcontext dec_ctx;
    CUcontext float_ctx;
    CUvideoctxlock dec_lock;
    CUstream dec_stm;


    DEC_PARSER parser;
    DEC_SOURCE source;

    int vid_width;
    int vid_height;

//allocated extent
    int nwidth;
    int nheight;
    int npitch;


    CUdeviceptr decoded_frames[2];
    void*decoded_frames_host[2];

//    CUVIDPARSERDISPINFO dispQ[10];

    int ofd;

    unsigned long long n_decoded_frames;
    int b_eod;//end of decode

    pthread_mutex_t mutex;
    pthread_cond_t cond;


};

typedef struct _DECODER_MAIN DECODER;









typedef struct{
        int        codecs;
        const char *name;
} vidFmt;


static vidFmt videoFormats[] =
{
    { cudaVideoCodec_MPEG1, "MPEG-1"  },
    { cudaVideoCodec_MPEG2, "MPEG-2"  },
    { cudaVideoCodec_MPEG4, "MPEG-4 (ASP)"  },
    { cudaVideoCodec_VC1,   "VC-1/WMV"  },
    { cudaVideoCodec_H264,  "AVC/H.264"  },
    { cudaVideoCodec_JPEG,  "M-JPEG"  },
    { cudaVideoCodec_NumCodecs,  "Invalid"  },
    { cudaVideoCodec_YUV420,"YUV  4:2:0"  },
    { cudaVideoCodec_YV12,  "YV12 4:2:0"  },
    { cudaVideoCodec_NV12,  "NV12 4:2:0"  },
    { cudaVideoCodec_YUYV,  "YUYV 4:2:2"  },
    { cudaVideoCodec_UYVY,  "UYVY 4:2:2"  },
    {                  -1 , "Unknown"  },

};



void print_picparam(CUVIDPICPARAMS*picpar)
{

    fprintf(stdout,"==========================\n");
    fprintf(stdout,"PicWidthInMbs: %d\n",picpar->PicWidthInMbs);
    fprintf(stdout,"FrameHeightInMbs: %d\n",picpar->FrameHeightInMbs);
    fprintf(stdout,"CurrPicIdx: %d\n",picpar->CurrPicIdx);
    fprintf(stdout,"Field_pic_flag: %d\n",picpar->field_pic_flag);
    fprintf(stdout,"bottom_field_flag: %d\n",picpar->bottom_field_flag);
    fprintf(stdout,"Second_field: %d\n",picpar->second_field);
    fprintf(stdout,"nBitstreamDataLen: %d\n",picpar->nBitstreamDataLen);
    fprintf(stdout,"pBitstreamData: %p\n",picpar->pBitstreamData);
    fprintf(stdout,"nNumSlices: %d\n",picpar->nNumSlices);
    fprintf(stdout,"pSliceDataOffsets: %p\n",picpar->pSliceDataOffsets);
    fprintf(stdout,"ref_pic_flag: %d\n",picpar->ref_pic_flag);
    fprintf(stdout,"intra_pic_flag: %d\n",picpar->intra_pic_flag);

    fprintf(stdout,"==========================\n");

}


void print_format(CUVIDEOFORMAT*fmt)
{

    fprintf(stdout,"Codec:%d\n",fmt->codec);
    fprintf(stdout,"Frame Rate %d/%d\n",fmt->frame_rate.numerator,fmt->frame_rate.denominator);
    fprintf(stdout,"Progressive?:%d\n",fmt->progressive_sequence);
    fprintf(stdout,"Coded Width:%d, Height:%d\n",fmt->coded_width,fmt->coded_height);

    fprintf(stdout,"Display_Area:{%d,%d,%d,%d}\n",
            fmt->display_area.left,fmt->display_area.top,
            fmt->display_area.right,fmt->display_area.bottom);

    fprintf(stdout,"Chroma_format: %d\n",fmt->chroma_format);

    fprintf(stdout,"DispAspecRatio:%d/%d\n",fmt->display_aspect_ratio.x, fmt->display_aspect_ratio.y);

    fprintf(stdout,"Video Signal Desc....\n");
    fprintf(stdout,"Video_Format:%d\n",fmt->video_signal_description.video_format);
    fprintf(stdout,"Color_Primaries:%d\n",fmt->video_signal_description.color_primaries);
    fprintf(stdout,"Transfer_Characteristics:%d\n",fmt->video_signal_description.transfer_characteristics);
    fprintf(stdout,"Matrix_Coefficients:%d\n",fmt->video_signal_description.matrix_coefficients);

    fprintf(stdout,"\nSeq header data length:%d\n",fmt->seqhdr_data_length);

}






int handle_video_source(void*userdata,CUVIDSOURCEDATAPACKET*packet)
{
#if 0 
    fprintf(stderr,"----handle_video_source\n");
    fprintf(stderr,"flags =%d\n",packet->flags);
    fprintf(stderr,"payload_siz =%ld\n",packet->payload_size);
    fprintf(stderr,"payload =%p\n",packet->payload);
    fprintf(stderr,"timestamp =%ld\n",packet->timestamp);
    fprintf(stderr,"----\n");
#else
    fprintf(stderr,"-");
#endif
    CUresult cures;
    DECODER*dec=(DECODER*)userdata;

    if(!dec->b_eod){

        cures=cuvidParseVideoData(dec->parser.vid_parser,packet);
//        CHK_CUDA(cures,"Parse Video Data");
        CHK_CUDA_ERR(cures);


        if((packet->flags & CUVID_PKT_ENDOFSTREAM)||(cures!=CUDA_SUCCESS)){
            fprintf(stderr,"WARN:: To Stop\n");

//            pthread_mutex_lock(&dec->mutex);
            dec->b_eod=1;
            pthread_cond_signal(&dec->cond);
//            pthread_mutex_unlock(&dec->mutex);
        }
    }
    return !dec->b_eod;

}



int init_video_source(DECODER*dec, const char*filename)
{
    if(!dec||!filename)
        return -1;
    CUresult cures;

    CUVIDSOURCEPARAMS srcpar;
    memset(&srcpar,0,sizeof srcpar);

    srcpar.pUserData=dec;
    srcpar.pfnVideoDataHandler=handle_video_source;
    srcpar.pfnAudioDataHandler=0;

    cures=cuvidCreateVideoSource(&dec->source.vid_source,filename,&srcpar);
    CHK_CUDA(cures,"Create Video Source");


    cures=cuvidGetSourceVideoFormat(dec->source.vid_source,&dec->source.vid_format,0);
    CHK_CUDA(cures,"Get Video Source's Format");

    dec->vid_width=dec->source.vid_format.coded_width;
    dec->vid_height=dec->source.vid_format.coded_height;

    fprintf(stderr,"Codec Width:%d,Height:%d\n",dec->vid_width,dec->vid_height);

    print_format(&dec->source.vid_format);

    fprintf(stderr,"<FILENAM::%s>\n",filename);

/*Display Dimensions..
    dec->dpy_width=dec->source.vid_format.display_area.right-<left>;
    ....
*/

    return 0;

}



void fini_video_source(DECODER*dec)
{
    cuvidDestroyVideoSource(&dec->source.vid_source);
}



static int _reset_(void *puserdata, CUVIDEOFORMAT *fmt)
{

    fprintf(stderr,"----------REset...\n");

    return 1;


}




static int _decode_frame(void *puserdata, CUVIDPICPARAMS *picpar)
{
//    fprintf(stderr,"----------To decode frame...\n");
    fprintf(stderr,"+");

    DECODER*dec=(DECODER*)puserdata;


    print_picparam(picpar);



    if(dec->b_eod)
        return 0;

    CUresult cures;
    cures=cuvidDecodePicture(dec->decoder,picpar);
    CHK_CUDA_ERR(cures);

    return 1;
}




static int _transfer_frame(void *puserdata, CUVIDPARSERDISPINFO *dispinfo)
{
//    fprintf(stderr,"----------To transfer frame...\n");
    fprintf(stderr,":");

    DECODER*dec=(DECODER*)puserdata;
    CUresult cures;
 
    
    cures=cuCtxPushCurrent(dec->float_ctx);
    CHK_CUDA(cures,"PUSH Context");

    CUVIDPROCPARAMS procpar;
    bzero(&procpar,sizeof(CUVIDPROCPARAMS));

    procpar.progressive_frame=dispinfo->progressive_frame;
//    procpar.second_field=0;
    procpar.top_field_first=dispinfo->top_field_first;
    procpar.unpaired_field=1;//progressive

    unsigned int mwidth=dec->vid_width;//dec->vid_width;
    unsigned int mheight=dec->vid_height;//dec->vid_height;
    unsigned int mpitch=0;

    fprintf(stderr,"DisplayInfo:index=%d; \n",dispinfo->picture_index);
/*
    fprintf(stderr,"Progressive_frame:%d\n",procpar.progressive_frame);
    fprintf(stderr,"Top Field First:%d\n",procpar.top_field_first);
    fprintf(stderr,"Unpaired Field:%d::::%d\n",procpar.unpaired_field,dispinfo->repeat_first_field);
*/
//    fprintf(stderr,"\n",);
//    fprintf(stderr,"\n",);
//    fprintf(stderr,"%p,    index:%d\t%p,\tpitch:%p,\tpar%p\n",&dec->decoder,dispinfo->picture_index,dec->decoded_frames[0],&mpitch,&procpar);
//    cures=cuvidMapVideoFrame(dec->decoder,dispinfo->picture_index,&dec->decoded_frames[0],&mpitch,&procpar);
#if 1

    cures=cuvidMapVideoFrame(dec->decoder,dispinfo->picture_index,&dec->decoded_frames[0],&mpitch,&procpar);
    CHK_CUDA_ERR(cures);
 
    mwidth=MEM_PAD(mwidth,0x3f);
    mheight=MEM_PAD(mheight,0x0f);


    unsigned int cpysiz=(mheight*mpitch*3)>>1;

    dec->nwidth=mwidth;
    dec->nheight=mheight;
    dec->npitch=mpitch;


    if(!dec->decoded_frames_host[0]){

        fprintf(stderr,"size::%u{mpitch:%u ,mheight:%u}\n",cpysiz,mpitch,mheight);
        
        cures=cuMemAllocHost(&dec->decoded_frames_host[0],dec->npitch*dec->nheight*3/2);
        CHK_CUDA(cures,"Alloc Memory Host");
    
        fprintf(stderr,"\033[33mHost:%p, Dev:%p,siz:%d\033[0m\n",dec->decoded_frames_host[0],
            (void*)dec->decoded_frames[0],cpysiz);
    }


    cures=cuMemcpyDtoH(dec->decoded_frames_host[0],dec->decoded_frames[0],cpysiz);
    CHK_CUDA_ERR(cures);

    int ret;
    ret=write(dec->ofd,dec->decoded_frames_host[0],cpysiz);
    CHK_RUNe(ret<0,"Write File..",return 0);

    dec->n_decoded_frames++;

    cures=cuvidUnmapVideoFrame(dec->decoder,dec->decoded_frames[0]);
    CHK_CUDA_ERR(cures);
#endif


    cures=cuCtxPopCurrent(&dec->float_ctx);
    CHK_CUDA(cures,"POP Context");


    return 1;

}






int init_video_parser(DECODER*dec)
{

    if(!dec)
        return -1;

    CUVIDPARSERPARAMS parpar;
    memset(&parpar,0,sizeof parpar);

    parpar.CodecType=dec->config->decode_type;
    parpar.ulMaxNumDecodeSurfaces=dec->config->maxsurfaces;//affact displayinfo's index//dec->config->;
    parpar.ulMaxDisplayDelay=1;
    parpar.pUserData=dec;

    parpar.pfnSequenceCallback=_reset_;//????;
    parpar.pfnDecodePicture=_decode_frame;//
    parpar.pfnDisplayPicture=_transfer_frame;//

    CUresult cures=cuvidCreateVideoParser(&dec->parser.vid_parser,&parpar);
    CHK_CUDA(cures,"Create VideoParser");

    return 0;
}


void fini_video_parser(DECODER*dec)
{


}




static size_t bytes2byte(size_t val,char*buff,int lbuff)
{
    static char* unit[]={"B","KiB","MiB","GiB","TiB","??????"};
    static int pos=0;
    if(val<=10000){
        snprintf(buff,lbuff,"%zu%s",val,unit[pos]);
        pos=0;
        return val;
    }else{
        val/=1024;
        pos++;
//        fprintf(stderr,"\033[33m[%zu]\n\033[0m",val);
        return bytes2byte(val,buff,lbuff);
    }
}

int _probe_cuda_info(void)
{

    CUresult cures;

    int n_dev=0;
    cures=cuDeviceGetCount(&n_dev);
    CHK_CUDA_ERR(cures);
    fprintf(stderr,"\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"\n");

    CUdevice cudev;
    int i;
    char devname[128]={0,};
    char membuff[32]={0,};
    size_t mem=0;
    for(i=0;i<n_dev;i++){
        cures=cuDeviceGet(&cudev,i);
        CHK_CUDA_ERR(cures);

        cures=cuDeviceGetName(devname,128,cudev);
        CHK_CUDA_ERR(cures);

        cures=cuDeviceTotalMem(&mem,cudev);
        CHK_CUDA_ERR(cures);
    
        mem=bytes2byte(mem,membuff,32);

        int val=0;
//        cures=cuDeviceGetAttribute(&val,CU_DEVICE_ATTRIBUTE_CAN_MAP_HOST_MEMORY,cudev);
        cures=cuDeviceGetAttribute(&val,CU_DEVICE_ATTRIBUTE_UNIFIED_ADDRESSING,cudev);
        CHK_CUDA_ERR(cures);


        fprintf(stderr,"-->DEV:%d{%s}<%d> :Memory:[%s]\n",i,devname,val,membuff);

    }



}



int init_cuda_resource(DECODER*dec)
{


    CUresult cures;

    cuInit(0);


    _probe_cuda_info();


    CUdevice device;
    CUcontext context;

    cures=cuDeviceGetCount(&dec->n_device);
    CHK_CUDA(cures,"GET Device Count.");

    fprintf(stderr,"Device Count::%d\n",dec->n_device);

    /*
     * Choose A Device.
     * */

    cures=cuDeviceGet(&dec->dec_dev,1);//pick 1 for test;
    CHK_CUDA(cures,"Choose Device");

    cures=cuCtxCreate(&dec->dec_ctx,0,dec->dec_dev);
    CHK_CUDA(cures,"Create Context");

    
    cures=cuCtxCreate(&dec->float_ctx,0,dec->dec_dev);
    CHK_CUDA(cures,"Create Context");

    cures=cuCtxPopCurrent(&dec->float_ctx);
    CHK_CUDA(cures,"POP Context");


    cures=cuvidCtxLockCreate(&dec->dec_lock,dec->dec_ctx);
    CHK_CUDA(cures,"Create Context Lock.");


    cures=cuStreamCreate(&dec->dec_stm, 0);
    CHK_CUDA(cures,"Create Stream");

/*
    cures=cuMemAllocHost(&dec->decoded_frames_host[0],dec->npitch*dec->nheight*3/2);
    CHK_CUDA(cures,"Alloc Memory Host");

*/



    return 0;
}




int init_decoder(DECODER*dec)
{

    CUresult nvres;

    CUVIDDECODECREATEINFO info;

    memset(&info,0,sizeof(CUVIDDECODECREATEINFO));

    info.CodecType=dec->config->decode_type;//cudaVideoCodec_H264;

    info.ulWidth=dec->config->width;
    info.ulHeight=dec->config->height;
    info.ulNumDecodeSurfaces=dec->config->maxsurfaces;//conf->num_decodersurfaces;

    info.ChromaFormat=cudaVideoChromaFormat_420;
    info.OutputFormat=cudaVideoSurfaceFormat_NV12;

    info.DeinterlaceMode=cudaVideoDeinterlaceMode_Adaptive;

    info.ulTargetWidth=info.ulWidth;
    info.ulTargetHeight=info.ulHeight;
    info.ulNumOutputSurfaces=dec->config->maxsurfaces;
#if 0
    if(format().codec==cudaVideoCodec_JPEG/MPEG2){
        info.ulCreationFlags=cudaVideoCreate_PreferCUDA;
    }else{
#endif   
    info.ulCreationFlags=cudaVideoCreate_PreferCUVID;
#if 0
    }
#endif



    info.vidLock=dec->dec_lock;


    pthread_mutex_init(&dec->mutex,NULL);

    pthread_mutex_init(&dec->cond,NULL);


#if 0
    cuInit(0);

    int n_device;
    CUdevice device;
    CUcontext context;

    cures=cuDeviceGetCount(&n_device);

    CHK_CUDA_ERR(cures);

    fprintf(stderr,"Device Count::%d\n",n_device);

    cures=cuDeviceGet(&device,3);

    CHK_CUDA_ERR(cures);

    cures=cuCtxCreate(&context,0,device);

    CHK_CUDA_ERR(cures);


#endif

    nvres=cuvidCreateDecoder(&dec->decoder,&info);
    CHK_CUDA(nvres,"Create Decoder");
//    fprintf(stderr,"RESULT::%d\n",nvres);





    return 0;

}


DECODER* init_nv_decoder(config)
    DEC_CONFIG *config;
{

    int ret=-1;

    DECODER*dec=(DECODER*)calloc(1,sizeof(DECODER));
    
    dec->config=config;

    ret=init_cuda_resource(dec);
    
    fprintf(stderr,"(%d)Init CUDA resource OK\n",ret);

    ret=init_decoder(dec);

    fprintf(stderr,"{%d}Init Encoder OK\n",ret);


    ret=init_video_source(dec,config->filename);
    fprintf(stderr,"(%d)Init video Source \n",ret);

    ret=init_video_parser(dec);

    fprintf(stderr,"(%d)Init Video Parser OK\n",ret);



    return dec;

}



DEC_CONFIG*get_default_config()
{
    
    DEC_CONFIG*pconf=calloc(1,sizeof(DEC_CONFIG));
    CHK_RUN(!pconf,"Alloc ConfigNode",return NULL);

    pconf->width=1280;
    pconf->height=720;
    pconf->maxsurfaces=4;
    pconf->decode_type=cudaVideoCodec_H264;
//    pconf->decode_type=cudaVideoCodec_MPEG2;
    

    return pconf;

}


void set_source_state(CUvideosource vidsrc)
{
    
    CUresult cures;

    cures=cuvidSetVideoSourceState(vidsrc,cudaVideoState_Started);
//    cures=cuvidSetVideoSourceState(vidsrc,cudaVideoState_Stopped);
//    cures=cuvidSetVideoSourceState(vidsrc,cudaVideoState_);
    CHK_CUDA_ERR(cures);

}






int main(int argc,char**argv)
{
   
    CUresult cures;
    char*filename="11.264";
    if(argc>1){
        filename=argv[1];
    }


    fprintf(stderr,"File<%s>\n",filename);

    int nr;
    int fd;
    fd=open(filename,O_RDONLY);
    CHK_RUNe(fd<0,"open video file",return EXIT_FAILURE);


    unsigned char*inbuff=calloc(1024,sizeof(char));

    CUVIDSOURCEDATAPACKET pkt;
    bzero(&pkt,sizeof pkt);
   

    DEC_CONFIG*conf=get_default_config();
   
    conf->filename=filename;

    DECODER*decoder=init_nv_decoder(conf);

    char ofile[128]={0,};
    strlcpy(ofile,filename,128);
    char*pdot=strrchr(ofile,'.');
    if(pdot && pdot!=ofile){
        *(pdot+1)=0;
    }else{
        ofile[strlen(ofile)]='.';
    }
    strlcat(ofile,"yuv",128);

    decoder->ofd=open(ofile,O_WRONLY|O_TRUNC|O_CREAT,0644);
    CHK_RUN(decoder->ofd<0,"Open outfile..",return EXIT_FAILURE);


#if 1
    set_source_state(decoder->source.vid_source);


#else
    long long ts=0;
    while((nr=read(fd,inbuff,1024)>0)){
        
        bzero(&pkt,sizeof pkt);
        pkt.flags=CUVID_PKT_TIMESTAMP;
        pkt.payload_size=nr;
        pkt.payload=inbuff;
        ts+=40;
        pkt.timestamp=ts;


        cures=cuvidParseVideoData(decoder->parser.vid_parser,&pkt);
        CHK_CUDA_ERR(cures);
//        fprintf(stderr,"Parser Loop ++\n");
        fprintf(stderr,"+");

        usleep(40);
    }
    if(nr==0){
        fprintf(stderr,"End of File\n");
        close(fd);
    }

#endif


    pthread_mutex_lock(&decoder->mutex);

    while(!decoder->b_eod)
        pthread_cond_wait(&decoder->cond,&decoder->mutex);

    fprintf(stderr,"-------++++++\n");
    pthread_mutex_unlock(&decoder->mutex);
//    pause();

    fprintf(stderr,"-------++++++\n");
    free(inbuff);
    close(decoder->ofd);
    fprintf(stderr,"-------++++++\n");

    return EXIT_SUCCESS;
}







/*
int handle_pic_decode(void*userdata,CUVIDPICPARAMARS*picpar)
{
    
    CUresult cures;
    DECODER*dec=(DECODER*)userdata;
}

*/



int get_source_state(CUvideosource*vidsrc)
{
    return cuvidGetVideoSourceState(vidsrc);
}




#if 0


int main(int argc,char**argv)
{

    CUresult nvres;
    CUresult cures;





    memset(&info,0,sizeof(CUVIDDECODECREATEINFO));

    info.CodecType=cudaVideoCodec_H264;

    info.ulWidth=1280;
    info.ulHeight=720;
    info.ulNumDecodeSurfaces=1;

    info.ChromaFormat=cudaVideoChromaFormat_420;
    info.OutputFormat=cudaVideoSurfaceFormat_NV12;

    info.DeinterlaceMode=cudaVideoDeinterlaceMode_Adaptive;

    info.ulTargetWidth=info.ulWidth;
    info.ulTargetHeight=info.ulHeight;

    if(format().codec==cudaVideoCodec_JPEG/MPEG2){
        info.ulCreationFlags=cudaVideoCreate_PreferCUDA;
    }else{
        
        info.ulCreationFlags=cudaVideoCreate_PreferCUVID;

    }



    cuInit(0);

    int n_device;
    CUdevice device;
    CUcontext context;

    cures=cuDeviceGetCount(&n_device);

    CHK_CUDA_ERR(cures);

    fprintf(stderr,"Device Count::%d\n",n_device);

    cures=cuDeviceGet(&device,3);

    CHK_CUDA_ERR(cures);

    cures=cuCtxCreate(&context,0,device);

    CHK_CUDA_ERR(cures);



    nvres=cuvidCreateDecoder(&decoder,&info);

    fprintf(stderr,"RESULT::%d\n",nvres);

    CHK_CUDA_ERR(nvres);


    return 0;

}


#endif 
