#include"NvEncoder_bbcv.h"
#include"NvHWEncoder.h"
#include<cuda.h>

#include<stdio.h>
#include<string.h>
#include"helper_cudadrv.h"
#include"helper_nvenc.h"

#include<stdlib.h>

#include<nvml.h>

#include"confile.h"


int GPU_SLICE_NUM_period = 100;
int GPU_SLICE_FLAG = 1;
int GPU_SLICE_NUM = 9;

#define SEP '.'
#define SEPARATOR_S "."
#define COMMENT_START_S "#"

//state 1 open 0 close
#define INTRAREFRESHOPENFLAG "intraRefresh_state"
#define INTRAREFRESHNUM "intraRefreshNumber"

typedef struct _NvEncoder{


    CNvHWEncoder *hwEncoder;
    EncodeConfig encCfg;
    EncodeBuffer encBuf;
    NvEncPictureCommand picCmd;

	NV_ENC_SEQUENCE_PARAM_PAYLOAD* sequenceParamPayload;

    CUdevice cuDevice;
    CUcontext cuContext;
    int devcnt;

/*
    uint8_t*yuv[3];
    int stride[3];
*/
}NvEncoder;


static int init_buffers(NvEncoder*enc)
{


    int ret=0;
    NVENCSTATUS nvResult;
    CUresult cuResult;


    NV_ENC_REGISTER_RESOURCE reg_res={0,};


    enc->encBuf.stInputBfr.dwWidth=enc->encCfg.maxWidth;
    enc->encBuf.stInputBfr.dwHeight=enc->encCfg.maxHeight;
    enc->encBuf.stInputBfr.bufferFmt=NV_ENC_BUFFER_FORMAT_NV12_PL;


    ////////////////////////////
    //Allocate INPUT Resource

    CUcontext cuCtxCur;
    CUdeviceptr devPtr;

    cuResult=cuMemAlloc(&devPtr,enc->encCfg.maxWidth*enc->encCfg.maxHeight*3/2);
    CHK_CUDA_ERR(cuResult);

    enc->encBuf.stInputBfr.pNV12devPtr=devPtr;
    enc->encBuf.stInputBfr.uNV12Stride=enc->encBuf.stInputBfr.dwWidth;

    enc->encBuf.stInputBfr.LumaSize=enc->encBuf.stInputBfr.uNV12Stride*enc->encBuf.stInputBfr.dwHeight;


//    fprintf(stderr,"CUDA INPUT BUFFER::Stride =%u\n",enc->encBuf.stInputBfr.uNV12Stride);
//    fprintf(stderr,"CUDA INPUT BUFFER::Width  =%u\n",enc->encBuf.stInputBfr.dwWidth);
//    fprintf(stderr,"CUDA INPUT BUFFER::Height =%u\n",enc->encBuf.stInputBfr.dwHeight);

#if 0
    void* phost;

    cuResult=cuMemAllocHost(&phost,enc->encBuf.stInputBfr.uNV12Stride,
            enc->encBuf.stInputBfr.dwHeight*3/2)
    CHK_CUDA_ERR(cuResult);


    enc->encBuf.stInputBfr.pNV12hostPtr=phost;
    enc->encBuf.stInputBfr.uNV12hostStride=enc->encBuf.stInputBfr.uNV12Stride;
#endif

    ///////////////////////
    //Using Mapped Resource..



    void*resource;
    nvResult=enc->hwEncoder->NvEncRegisterResource(NV_ENC_INPUT_RESOURCE_TYPE_CUDADEVICEPTR,(void*)enc->encBuf.stInputBfr.pNV12devPtr,
            enc->encBuf.stInputBfr.dwWidth,enc->encBuf.stInputBfr.dwHeight,enc->encBuf.stInputBfr.uNV12Stride,&resource);

    CHK_NVENC_ERR(nvResult);

    enc->encBuf.stInputBfr.nvRegisteredResource=resource;

    void* insurf;
    nvResult=enc->hwEncoder->NvEncMapInputResource(enc->encBuf.stInputBfr.nvRegisteredResource,&insurf);

    CHK_NVENC_ERR(nvResult);

    enc->encBuf.stInputBfr.hInputSurface=insurf;



    ////////////////////////////
    //Allocate OUTPUT Resource

    enc->encBuf.stOutputBfr.dwBitstreamBufferSize=1024*1024;//1MiB
    nvResult=enc->hwEncoder->NvEncCreateBitstreamBuffer(enc->encBuf.stOutputBfr.dwBitstreamBufferSize,&enc->encBuf.stOutputBfr.hBitstreamBuffer);
    CHK_NVENC_ERR(nvResult);


    enc->encBuf.stOutputBfr.bEOSFlag=false;
    enc->encBuf.stOutputBfr.bWaitOnEvent=false;





    return ret;

}



static int fini_buffers(NvEncoder*enc)
{
    
    int ret;
    NVENCSTATUS nvResult;

    //1.free device memory

    cuMemFree((CUdeviceptr)enc->encBuf.stInputBfr.pNV12devPtr);
//    cuMemFreeHost();

    //2.free input resource
    nvResult=enc->hwEncoder->NvEncUnmapInputResource(enc->encBuf.stInputBfr.hInputSurface);
    CHK_NVENC(nvResult,"Unmap InputResource");

    nvResult=enc->hwEncoder->NvEncUnregisterResource(enc->encBuf.stInputBfr.nvRegisteredResource);
    CHK_NVENC(nvResult,"Unregister Resource");


    //3.free output resoruce

    nvResult=enc->hwEncoder->NvEncDestroyBitstreamBuffer(enc->encBuf.stOutputBfr.hBitstreamBuffer);
    CHK_NVENC(nvResult,"Destroy BitstreamBuffer");


    return 0;
}

static void check_nvml_error(nvmlReturn_t nvret,const char*msg,int line)
{

    if(nvret!=NVML_SUCCESS){

        fprintf(stderr,"NVML:@%d::%s:{%s}\n",line,msg,nvmlErrorString(nvret));
        exit(1);
    }

}

#define CHK_NVML(ret,message) \
    check_nvml_error(ret,message,__LINE__)



typedef struct _stats{
   
    nvmlDevice_t handler;
    nvmlMemory_t meminfo;
    nvmlUtilization_t utils;

    unsigned int encutil;
    unsigned int decutil;


}devstat;


int bytes_to(unsigned long long val,int u,char*ounit)
{
    static const char*unit[]={"Byte","KiB","MiB","GiB"};
    
    if(val<9999){
        strcpy(ounit,unit[u]);
        return val;
    }else{
        bytes_to(val/1024,u+1,ounit);
    }

}
int conv_bytes(unsigned long long val,char*unit)
{
    int v;
    int u=0;
    v=bytes_to(val,0,unit);
    return v;
}



void print_devstats(devstat*s)
{


    printf("==========\n");
    
    printf("Meminfo::\ttotal:%llu\tused:%llu\tfree:%llu\n",s->meminfo.total,s->meminfo.used,s->meminfo.free);



    printf("Utils  ::\tgpu:%u\tmemory:%u\n",s->utils.gpu,s->utils.memory);

    printf("Encodec::\tencoder:%u\tdecoder:%u\n",s->encutil,s->decutil);
    printf("----------\n");
}


int probe_gpustats(devstat**stats)
{

    unsigned int n_dev;
    nvmlReturn_t nvret;


    nvret=nvmlInit();
    CHK_NVML(nvret,"Init NVML");


    nvret=nvmlDeviceGetCount(&n_dev);
    CHK_NVML(nvret,"getCount");


    *stats=(devstat*)calloc(n_dev,sizeof(devstat));
    devstat*pstats=*stats;


    int i;
    for(i=0;i<n_dev;i++)
        nvmlDeviceGetHandleByIndex(i,&pstats[i].handler);

    
    for(i=0;i<n_dev;i++)
        nvmlDeviceGetMemoryInfo(pstats[i].handler,&pstats[i].meminfo);
    
    for(i=0;i<n_dev;i++)
        nvmlDeviceGetUtilizationRates(pstats[i].handler,&pstats[i].utils);

    unsigned int sampp;
    for(i=0;i<n_dev;i++)
        nvmlDeviceGetEncoderUtilization(pstats[i].handler,&pstats[i].encutil,&sampp);

    for(i=0;i<n_dev;i++)
        nvmlDeviceGetDecoderUtilization(pstats[i].handler,&pstats[i].decutil,&sampp);
#if 0
    int maxfreeind=0;
    int maxfree=0;
    for(i=0;i<n_dev;i++){

        print_devstats(&pstats[i]);

        int free=pstats[i].meminfo.free; 
//        fprintf(stderr,"<%d\n",free);
        if(free>maxfree){
            maxfree=free;
            maxfreeind=i;
        }

    }
#endif
    nvret=nvmlShutdown();
    CHK_NVML(nvret,"Shutdown NVML");


    return n_dev;
}

int get_core_by_mem(devstat*ps,int n_dev)
{
    int maxfreeind=0;
    long long maxfree=0;
    int i;
    for(i=0;i<n_dev;i++){

        print_devstats(&ps[i]);

        long long free=ps[i].meminfo.free; 
//        fprintf(stderr,"<%d\n",free);
        if(free>maxfree){
            maxfree=free;
            maxfreeind=i;
        }

    }
  
    return maxfreeind;

}


int get_next_core(void)
{
    
    devstat*ds;
    int n;
    n=probe_gpustats(&ds);

    n=get_core_by_mem(ds,n);  

    free(ds);

    return n;
}

static int init_cuda(NvEncoder*enc)
{

    NVENCSTATUS nvResult;
    CUresult cuResult;
    int devid;



    cuResult=cuInit(0);
    CHK_CUDA_ERR(cuResult);

    cuResult=cuDeviceGetCount(&enc->devcnt);
    CHK_CUDA_ERR(cuResult);

    fprintf(stderr,"Device Count = %d\n",enc->devcnt);

    devid=enc->encCfg.deviceID;//default is 0

    if(devid==-1 || devid>=enc->devcnt){
	    devid=get_next_core();
	    fprintf(stderr,"\033[33mAuto Select DevID:= %d\n\033[0m",devid);
	    enc->encCfg.deviceID=devid;

	}else{
	    fprintf(stderr,"\033[33mGiven DevID:= %d\n\033[0m",devid);
	}

    cuResult=cuDeviceGet(&enc->cuDevice,devid);
    CHK_CUDA_ERR(cuResult);


    cuResult=cuCtxCreate(&enc->cuContext,0,enc->cuDevice);
    CHK_CUDA_ERR(cuResult);

    return 0;

}


int fini_cuda(NvEncoder*enc)
{

    CUresult cuResult;
    cuResult=cuCtxDestroy(enc->cuContext);
    CHK_CUDA(cuResult,"cuda Context Destroy");

    return 0;
}




int NvEncoder_InitDefaultConfig(EncodeConfig *encCfg)
{
	memset(encCfg,0,sizeof(EncodeConfig));
	
    encCfg->gopsize=100;
    encCfg->intraRefreshEnableFlag=1;
   	encCfg->intraRefreshPeriod=100;  //100 I 帧分片
    encCfg->intraRefreshDuration=GPU_SLICE_NUM; //I 帧分片
	//4
    
    encCfg->width=1280;
   	encCfg->height=720;
    encCfg->maxWidth=1280;
    encCfg->maxHeight=720;

    encCfg->bitrate=4000000;
    encCfg->rcMode= NV_ENC_PARAMS_RC_CBR; //NV_ENC_PARAMS_RC_VBR;//NV_ENC_PARAMS_RC_CBR;
    encCfg->gopLength=NVENC_INFINITE_GOPLENGTH;
    encCfg->deviceType=NV_ENC_DEVICE_TYPE_CUDA;
    encCfg->codec=NV_ENC_H264;
    encCfg->fps=25;
    encCfg->qp=28;
    encCfg->encoderPreset=(char*)"hp";//"lowLatencyHP";    //"hp";
    encCfg->encoderProfile=(char*)"main";
	encCfg->pictureStruct=NV_ENC_PIC_STRUCT_FRAME;
    encCfg->isYuv444=0;
	encCfg->repeatSPSPPS = 1;

	
	return 0;
}


int Get_conf_refreshflag()
{
	//get default config
	FILE* fd_nvsdk = NULL;
	char filename[1024]={0};
	strcpy(filename,"/etc/bbcv_nvsdk.conf");

#if 0	
	fd_nvsdk = fopen(filename,"r");
	if(NULL == fd_nvsdk)
		return 0;
	int lsiz=512*sizeof(char);
    char*line=(char*)malloc(lsiz);
    bzero(line,lsiz);

	char*key,*value;
	int nit;
	confile_t*pconf=confile_new(filename);
	cf_node_t*pnode;
	printf("ready to get config data from file:%s\n",filename);
    while(fgets(line,lsiz,fd_nvsdk)){
        
        nit=_parse_line(line,&key,&value);
        if(!nit)
            continue;

        pnode=confile_set_config(pconf,key,value);
      	
    }
#endif
	printf("ready to get config data from file:%s\n",filename);
   	
	 confile_t*config=confile_parse_file(filename);
	if(config==NULL)
	 	return 0;

    cf_node_t*node=confile_get_config(config,INTRAREFRESHOPENFLAG,NULL);
    if(node)
      GPU_SLICE_FLAG = atoi(node->cfn_val);
	node=confile_get_config(config,INTRAREFRESHNUM,NULL);
    if(node)
      GPU_SLICE_NUM = atoi(node->cfn_val);

	printf("ready to get config data %s=%d, %s=%d \n",INTRAREFRESHOPENFLAG,
		GPU_SLICE_FLAG,INTRAREFRESHNUM,GPU_SLICE_NUM);
   
	return 0;

}


int NvEncoder_Initialize(nvEncodeHandle* pHandle,char argc,char**argv)
{
    NVENCSTATUS nvResult;
    int ret;
	pHandle->phwEncoder = malloc(sizeof(NvEncoder));

	NvEncoder *enc = (NvEncoder*)pHandle->phwEncoder ;

    memset(enc,0,sizeof *enc);
    enc->hwEncoder=new CNvHWEncoder;

#if 0
    enc->encCfg.gopsize=100;
    enc->encCfg.intraRefreshEnableFlag=1;
    enc->encCfg.intraRefreshPeriod=100;
    enc->encCfg.intraRefreshDuration=4;
    
    enc->encCfg.width=1280;
    enc->encCfg.height=720;
    enc->encCfg.maxWidth=1280;
    enc->encCfg.maxHeight=720;

    enc->encCfg.bitrate=4000000;
    enc->encCfg.rcMode=NV_ENC_PARAMS_RC_CBR;
    enc->encCfg.gopLength=NVENC_INFINITE_GOPLENGTH;
    enc->encCfg.deviceType=NV_ENC_DEVICE_TYPE_CUDA;
    enc->encCfg.codec=NV_ENC_H264;
    enc->encCfg.fps=25;
    enc->encCfg.qp=28;
    enc->encCfg.encoderPreset=(char*)"hp";
    enc->encCfg.encoderProfile=(char*)"main";
    enc->encCfg.pictureStruct=NV_ENC_PIC_STRUCT_FRAME;
    enc->encCfg.isYuv444=0;
#endif
	memcpy(&enc->encCfg,&pHandle->encCfg,sizeof(EncodeConfig));
    nvResult=enc->hwEncoder->ParseArguments(&enc->encCfg,argc,argv);
    CHK_NVENC(nvResult,"Parse Encoder Arguments.");

	Get_conf_refreshflag();

	//enc->encCfg.repeatSPSPPS = 1;
	enc->encCfg.intraRefreshPeriod=GPU_SLICE_NUM_period;//enc->encCfg.gopsize;//using gopsize instead of IRP for output I frame;
	enc->encCfg.intraRefreshDuration=GPU_SLICE_NUM;
	enc->encCfg.gopLength=NVENC_INFINITE_GOPLENGTH;

	//enc->encCfg.gopLength = enc->encCfg.gopsize;
	//enc->encCfg.gopsize0 = 12;
	//enc->encCfg.deviceID = 0;
//	enc->encCfg.vbvMaxBitrate = enc->encCfg.bitrate*3/2;
	// enc->encCfg.qp=20;
//	enc->encCfg.vbvSize = enc->encCfg.bitrate;
//	 enc->encCfg.rcMode= NV_ENC_PARAMS_RC_2_PASS_FRAMESIZE_CAP;
	 //NV_ENC_PARAMS_RC_2_PASS_VBR;  有效降低了第一帧的大小
	if(GPU_SLICE_FLAG==0)
	{
		enc->encCfg.intraRefreshPeriod=0;//enc->encCfg.gopsize;//using gopsize instead of IRP for output I frame;
	    enc->encCfg.intraRefreshDuration=0;
		enc->encCfg.gopLength=enc->encCfg.gopsize;
	}
	
    ret=init_cuda(enc);
    if(ret!=0)
        return -6;

	fprintf(stderr,"\033[33m malloc paramload 1\n\033[0m");
	enc->sequenceParamPayload = (NV_ENC_SEQUENCE_PARAM_PAYLOAD*)malloc(sizeof(NV_ENC_SEQUENCE_PARAM_PAYLOAD));
	memset(enc->sequenceParamPayload,0,sizeof(NV_ENC_SEQUENCE_PARAM_PAYLOAD));
	int spsppsSize = 1024;
	enc->sequenceParamPayload->spsId = 0;
	enc->sequenceParamPayload->ppsId = 0;
	enc->sequenceParamPayload->version = NV_ENC_LOCK_BITSTREAM_VER;
	enc->sequenceParamPayload->inBufferSize = spsppsSize;
	fprintf(stderr,"\033[33m malloc paramload 3 \n\033[0m");
	enc->sequenceParamPayload->outSPSPPSPayloadSize = (uint32_t*)malloc(sizeof(uint32_t));
	*(enc->sequenceParamPayload->outSPSPPSPayloadSize) = spsppsSize;
	//NV_ENC_SEQUENCE_PARAM_PAYLOAD
	enc->sequenceParamPayload->spsppsBuffer = malloc(sizeof(char)*spsppsSize);
	
	fprintf(stderr,"\033[33m malloc paramload 4 \n\033[0m");


    nvResult=enc->hwEncoder->Initialize((void*)enc->cuContext,(NV_ENC_DEVICE_TYPE)enc->encCfg.deviceType);
    CHK_NVENC(nvResult,"Initialize HW Encoder.");

    GUID encodeGUID;

    encodeGUID=enc->hwEncoder->GetEncodeGUID(enc->encCfg.codec);
    enc->encCfg.presetGUID=enc->hwEncoder->GetPresetGUID(enc->encCfg.encoderPreset,NV_ENC_H264);
    enc->encCfg.profileGUID=enc->hwEncoder->GetProfileGUID(enc->encCfg.encoderProfile,NV_ENC_H264);


    
    nvResult=enc->hwEncoder->CreateEncoder(&enc->encCfg);
    CHK_NVENC(nvResult,"CreateEncoder");

	fprintf(stderr,"\033[33m malloc paramload 4 \n\033[0m");


    ret=init_buffers(enc);
    if(ret!=0)
        return -8;

    return 0;

}

int Find_H264_IFrameHead(unsigned char* buff,int ilen)
{
	int iPos = -1;
	unsigned char *pFrameBuff = buff;
	int iCmpLen = 0;
	while(iCmpLen < (min(ilen-4,1024 )) )
	{
		if(*pFrameBuff == 0x00 && *(pFrameBuff+1)==0x00 
			&& *(pFrameBuff+2)==0x01 && *(pFrameBuff+3)==0x65 )
		{
			//Group of picture header
			iPos = iCmpLen;
			break;
		}
		++pFrameBuff;
		++iCmpLen;
	}

	return iPos;//>=0则找到
}


FILE *fp = NULL;
int NvEncoder_EncodeFrame(nvEncodeHandle* pHandle,char*nv12,char*oes)
{
    
    NVENCSTATUS nvResult;
    CUresult cuResult;

	NvEncoder *enc = (NvEncoder*)pHandle->phwEncoder ;
//    1.Copy nv12 to device 

    
    cuResult=cuMemcpyHtoD((CUdeviceptr)enc->encBuf.stInputBfr.pNV12devPtr,nv12,
                            enc->encBuf.stInputBfr.uNV12Stride*enc->encBuf.stInputBfr.dwHeight*3/2);
	int iIntervalStart = enc->encCfg.gopsize0;

	int iFlag = 0;

	if(GPU_SLICE_FLAG == 0)
	{
	    if(enc->hwEncoder->m_EncodeIdx>=(enc->encCfg.fps*5))
		{
	//        enc->hwEncoder->m_EncodeIdx=0;
			uint32_t ilasttotal = ((enc->encCfg.fps*5)/iIntervalStart)*iIntervalStart;

			uint32_t index = enc->hwEncoder->m_EncodeIdx-ilasttotal;
	        if(index%enc->encCfg.gopsize==0){
	            enc->picCmd.bForceIDR=true;
				//printf("---set idr index=%d ,encodeidx=%d \n",index,enc->hwEncoder->m_EncodeIdx);
	        }else{
	            enc->picCmd.bForceIDR=false;
	        }
       	
     // enc->picCmd.bForceIDR=false;
        
           // enc->picCmd.bForceIntraRefresh=false;
           // enc->picCmd.intraRefreshDuration=0;
    	}else{

	        if(enc->hwEncoder->m_EncodeIdx%iIntervalStart==0){
	            //enc->picCmd.bForceIntraRefresh=true;
	            //enc->picCmd.intraRefreshDuration=enc->encCfg.gopsize0;
	            enc->picCmd.bForceIDR=true;
				//printf("---set idr encodeidx=%d \n",enc->hwEncoder->m_EncodeIdx);
	        }else{
	        	enc->picCmd.bForceIDR=false;
	           // enc->picCmd.bForceIntraRefresh=false;
	          //  enc->picCmd.intraRefreshDuration=0;
	        }

    	}
	}
	else
	{	
	    if(enc->hwEncoder->m_EncodeIdx>=(enc->encCfg.fps*5)){
			uint32_t index = enc->hwEncoder->m_EncodeIdx;
	        if(index%enc->encCfg.intraRefreshPeriod==0){         
	            enc->picCmd.bForceIntraRefresh=true;
	          	enc->picCmd.intraRefreshDuration=GPU_SLICE_NUM;  //enc->encCfg.gopsize;
				//printf("---set idr index=%d ,encodeidx=%d \n",index,enc->hwEncoder->m_EncodeIdx);
	        }else{
	            enc->picCmd.bForceIDR=false;
	            enc->picCmd.bForceIntraRefresh=false;
	          	enc->picCmd.intraRefreshDuration=0;
	        }

	    }else{

	        if(enc->hwEncoder->m_EncodeIdx%iIntervalStart==0){
	            enc->picCmd.bForceIDR=true;

	        }else{
	        	enc->picCmd.bForceIDR=false;
	        }

	    }
	}
	//if(enc->hwEncoder->m_EncodeIdx >enc->encCfg.fps*5 && enc->hwEncoder->m_EncodeIdx %	(enc->encCfg.gopsize) == enc->encCfg.gopsize-1 )  
	if(enc->hwEncoder->m_EncodeIdx %	(25) == 25-1 )   
		iFlag = 1;
	
    nvResult=enc->hwEncoder->NvEncEncodeFrame(&enc->encBuf,&enc->picCmd,
            enc->hwEncoder->m_uCurWidth,
            enc->hwEncoder->m_uCurHeight,
            NV_ENC_PIC_STRUCT_FRAME);

    CHK_NVENC(nvResult,"EncodeFrame");

    NV_ENC_LOCK_BITSTREAM bstream={0,};
    bstream.version=NV_ENC_LOCK_BITSTREAM_VER;
    bstream.outputBitstream=enc->encBuf.stOutputBfr.hBitstreamBuffer;
    bstream.doNotWait=enc->encBuf.stOutputBfr.bWaitOnEvent;


    int  eslen;

    nvResult=enc->hwEncoder->NvEncLockBitstream(&bstream);
    CHK_NVENC(nvResult,"Lock Bitstream");
	eslen=bstream.bitstreamSizeInBytes;

	if(GPU_SLICE_FLAG==0)
	{
		eslen=bstream.bitstreamSizeInBytes;
		
		memcpy(oes,bstream.bitstreamBufferPtr,eslen);

	}
	else
	{
		if(enc->hwEncoder->m_EncodeIdx == 1)
		{
			//第一帧
			printf("-----first  spslen =%d \n",*enc->sequenceParamPayload->outSPSPPSPayloadSize);
			NVENCSTATUS ret = enc->hwEncoder->NvEncGetSequenceParams(enc->sequenceParamPayload);	
			printf("-----hello spslen =%d \n",*enc->sequenceParamPayload->outSPSPPSPayloadSize);
			//printf("-----not find I frame spslen =%d \n",*enc->sequenceParamPayload->outSPSPPSPayloadSize);
			eslen=bstream.bitstreamSizeInBytes;

			memcpy(oes,bstream.bitstreamBufferPtr,eslen);
		}
		else
		{
			//查找每一个I 帧
		//	printf("-----begin find I frame spslen =%d \n",*enc->sequenceParamPayload->outSPSPPSPayloadSize);
			if(iFlag)
			{
				printf("-----find I frame spslen =%d \n",*enc->sequenceParamPayload->outSPSPPSPayloadSize);
				//找到I帧
				eslen=bstream.bitstreamSizeInBytes;
				int ippslen = *(enc->sequenceParamPayload->outSPSPPSPayloadSize);
				memcpy(oes,enc->sequenceParamPayload->spsppsBuffer,ippslen);
				printf("---ippslen =%d eslen=%d\n",ippslen,eslen);
				if(eslen >0){
	    			memcpy(oes+ippslen,bstream.bitstreamBufferPtr,eslen);
					eslen=bstream.bitstreamSizeInBytes+ippslen;
				}
				else{
					//no es data
					eslen=ippslen;
				}
			}
			else
			{
				//printf("-----not find I frame spslen =%d \n",*enc->sequenceParamPayload->outSPSPPSPayloadSize);
				eslen=bstream.bitstreamSizeInBytes;
				if(eslen >0){
	    			memcpy(oes,bstream.bitstreamBufferPtr,eslen);
				}
			}
		}
	}
    nvResult=enc->hwEncoder->NvEncUnlockBitstream(enc->encBuf.stOutputBfr.hBitstreamBuffer);
    CHK_NVENC(nvResult,"Unlock Bitstream");


    return eslen;




	
}


int NvEncoder_Finalize(nvEncodeHandle* pHandle)
{
	NvEncoder *enc = (NvEncoder*)pHandle->phwEncoder;
	free(enc->sequenceParamPayload->spsppsBuffer);
	free(enc->sequenceParamPayload->outSPSPPSPayloadSize);
	free(enc->sequenceParamPayload);
    delete enc->hwEncoder;
}



