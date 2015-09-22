
#ifndef _H_NVENCODER_
#define _H_NVENCODER_

#include"NvHWEncoder.h"
#include<cuda.h>

#ifdef __cplusplus
extern "C" {
#endif




typedef struct _nvEncoder{
	void* phwEncoder;
	EncodeConfig encCfg;
	NvEncPictureCommand picCmd;
}nvEncodeHandle;


int NvEncoder_InitDefaultConfig(EncodeConfig *encCfg);


int NvEncoder_Initialize(nvEncodeHandle *enc,char argc,char**argv);


int  NvEncoder_EncodeFrame(nvEncodeHandle*enc,char*nv12,char*oes);


int NvEncoder_Finalize(nvEncodeHandle*enc);


#ifdef __cplusplus
}
#endif



#endif
