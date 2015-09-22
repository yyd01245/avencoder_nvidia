/*----------------------------------------------------------------------------------------------
*
* This file is ArcSoft's property. It contains ArcSoft's trade secret, proprietary and       
* confidential information. 
* 
* The information and code contained in this file is only for authorized ArcSoft employees 
* to design, create, modify, or review.
* 
* DO NOT DISTRIBUTE, DO NOT DUPLICATE OR TRANSMIT IN ANY FORM WITHOUT PROPER AUTHORIZATION.
* 
* If you are not an intended recipient of this file, you must not copy, distribute, modify, 
* or take any action in reliance on it. 
* 
* If you have received this file in error, please immediately notify ArcSoft and 
* permanently delete the original and any copy of any file and any printout thereof.
*
*-------------------------------------------------------------------------------------------------*/
 
/*************************************************************************************************
**
**      Copyright (c) 2004 by ArcSoft Inc.
**
**      Name                   : ASVEncoder_interface.h
**
**      Purpose                : Provide encoder interface for app level.
**
**      Additional             : To use our encoder, the app only need this header file
								 with a lib. Soucecode need not be shown to app level.
**
**------------------------------------------------------------------------------------------------
**
**      Maintenance milestone:
**			2014/08/08		Created by steffi wang
**************************************************************************************************/
 

#ifndef __ASVENCODER_INTERFACE_
#define __ASVENCODER_INTERFACE_

#if defined(WIN32) || defined(WIN64)
#define ASVE_API __stdcall
#define ASVE_ENCODER_DLL "ASVEncoder.dll"
#elif defined(_MACOS) || defined(linux)
#define ASVE_API
#define ASVE_ENCODER_DLL "ASVEncoder.so"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _DATATYPE_
#define _DATATYPE_
typedef unsigned char		UInt8;
typedef char				Int8;
typedef unsigned short		UInt16;
typedef short				Int16;
typedef unsigned int		UInt32;
typedef int				    Int32;

typedef long				Bool;
typedef void				Void;
typedef float               Float;
typedef double				Double;
#ifdef WIN32
typedef	__int64				Int64;
typedef unsigned __int64	UInt64;
typedef long				Long;
#elif defined(_MACOS) || defined(linux)
typedef long long			Int64;
typedef unsigned long long	UInt64;
typedef long				Long;
#else
typedef long long			Int64;
typedef unsigned long long	UInt64;
typedef long				Long;
#endif
#ifdef WIN32
typedef TCHAR 				ARC_TCHAR;
#elif defined(_MACOS) || defined(linux)
typedef char 				ARC_TCHAR;
#endif
#endif //_DATATYPE_

#define ASVEHANDLE	Void *

/*
 *	define some error code
 */
#define ASVE_OK						0
#define ASVEGENERAL_ERROR			-1
#define ASVEPARAMETER_INVALID		-2
#define ASVEPARAMETER_NOT_SUPPORT	-3
#define ASVEMEMORY_ERROR			-4
#define ASVEINSUFFICIENT_BUFFER		-5
#define ASVECREATE_ENCODER_ERROR    -6
#define ASVEEXPIRED_ERROR           -10
#define ASVESLICE_NUM_ERROR      	-11

//value for profile
#define H264_BASELINE_PROFILE 66
#define H264_MAIN_PROFILE 77
#define H264_HIGH_PROFILE 100

//value for frame type

#define FRAME_TYPE_P      		0 /**< Forward predicted */
#define FRAME_TYPE_B      		1 /**< Bi-directionally predicted picture */
#define FRAME_TYPE_I      		2 /**< Intra predicted picture */
#define FRAME_TYPE_IDR		    3 /**< IDR picture */
#define FRAME_TYPE_UNKNOWN   	0xFF /**< Picture type unknown */

//value for video quality level
#define QUALITY_LEVEL_NULL		0
#define QUALITY_LEVEL_BEST		1
#define QUALITY_LEVEL_GOOD		2
#define QUALITY_LEVEL_BALANCE	3
#define QUALITY_LEVEL_FAST		4
#define QUALITY_LEVEL_FASTER	5
#if 0
typedef struct tag_ASVEncoderInSample
{
	Int64            	llStart;
	Int64            	llEnd;
	UInt32				width;
	UInt32				height;
	UInt32				stride;
	UInt32				vstride;
	UInt32       		ulBufMode;//0:SW;1:HW
	UInt32        		ulColorSpace; //0:NV12;1:YV12;2:I420
	UInt8           	*pbYUV[3]; //original data YUV
	UInt8  				reserved[64];
} ASVEncoderInSample;
#endif



typedef struct tag_ASVEncoderRect
{

  UInt32                left;
  UInt32                top;
  UInt32                width;
  UInt32                height;

} ASVEncoderRect;


#define RESERVED_SIZE 64 - sizeof(ASVEncoderRect *)
 

typedef struct tag_ASVEncoderInSample
{
     Int64                 llStart;
     Int64                 llEnd;
     UInt32                width;
     UInt32                height;
     UInt32                stride;
     UInt32                vstride;
     UInt32                ulBufMode;//0:SW;1:HW
     UInt32                ulColorSpace; //0:NV12;1:YV12;2:I420;3:XRGB
     //Original data for YUV in pbYUV[3] if ulColorSpace=1~3 <0-2>;
     //or original data for XRGB in pbYUV[0] if ulColorSpace=3
     UInt8                 *pbYUV[3];
     //Only valid when ulColorSpace=4
     //If pRect=NULL means pbYUV[0] is original data
     //If pRect has valid value that means pbYUV[0] is difference data
     ASVEncoderRect        *pRect;
     UInt8                 reserved[RESERVED_SIZE];

} ASVEncoderInSample;

 



typedef struct tag_ASVEncoderOutSample
{
	Int64         		llStart;
	Int64         		llEnd;
	Void			 	*pbBuffer;  //[O] the encoded data	
	Long             	lBufSize; //[O] the length of encoded data
	UInt32     			lFrameType; //[O] to indicate the frametype of this frame
	UInt8  				reserved[64];
} ASVEncoderOutSample;

/**********************************************************
*	ASVEDeliveryNotifyPtr
*	Discription:	This function is a callback to get encodered frame
*	Parameter:		
* 		caller[in]:
*		pUserData[in]:
*		pSample[out]:	The pointer to encodered frame
*	Return:
*		0:						Succeed.
*		PARAMETER_INVALID:		The input parameter is invalid.
*		PARAMETER_NOT_SUPPORT:	Only CAVLC is supported temporarily, if pPar->bCAVLC is set equal 0, 
*								PARAMETER_NOT_SUPPORT will be return.
*		MEMORY_ERROR:			Allcate memory for encoder handle failed
*	Remark:
*		App must provide it to get encodered frame.
**********************************************************/
typedef Int32 (*ASVEDeliveryNotifyPtr)(void *caller, void *pUserData, ASVEncoderOutSample *pSample);
typedef struct tag_ASVENCODERINTERFACE
{
	void *caller;
	void *pusedata;
	ASVEDeliveryNotifyPtr pInterface;
} ASVEncoderInterface;

typedef struct tag_ASVEncoderParam
{
	Int32  video_format_type;
	Int32  width;
	Int32  height;
	Int32  stride;//not used yet
	Int32  vstride;//not used yet
	Int32  aspect_ratio_y; 
	Int32  aspect_ratio_x;
	Double frame_rate;
	
	Int32  profile;
	Int32  level;

	Int32  idr_interval;
	Int32  successive_bframes;
	Int32  num_slices;//default:1; only support 1 by now
	Int32  thread_count;//default:1; only support 1 by now
	Int32  closed_gop;
	Int32  enable_adaptive_gop;

	Int32  entropy_method;
	Int32  enable_deblock; 
	Int32  maxnum_ref_forward;

	Int32  interlace;//0:frame; 1: mbaff; 2: field; only support 0 and 2 by now
	Int32  top_field_first;

	Int32  video_quality_level;//default:2; from 0 - 4, 4 is the best

	Int32  rate_control_type;//CUDA:0-FQ, 1-CBR, 2-VBR
	Int32  qp_i;//0-51
	Int32  qp_p;//not used yet
	Int32  qp_b;//not used yet

	Int32  target_bit_rate;//unit in kbps
	Int32  max_bit_rate;//unit in kbps
	Int32  bit_rate_buffer_size;//unit in kbps
	   
	Double delay;//unit in seconds		

	Int32 hDevice;
	Void *pContext;//ignore by now
	Void *pMutex;//ignore by now
	Int32 bUseContext;//ignore by now
	
	Int32 use_aud_unit;
	
	ASVEncoderInterface callback;

	UInt8  reserved[64];
} ASVEncoderParam;

/**********************************************************
*	ASVE_Create
*	Discription:	This function is called to create an encoder handle
*	Parameter:		
*		pPar[in]:		The pointer to encoder parameters.
*		handle[out]:	The pointer to encoder handle
*	Return:
*		0:						Succeed.
*		PARAMETER_INVALID:		The input parameter is invalid.
*		PARAMETER_NOT_SUPPORT:	Only CAVLC is supported temporarily, if pPar->bCAVLC is set equal 0, 
*								PARAMETER_NOT_SUPPORT will be return.
*		MEMORY_ERROR:			Allcate memory for encoder handle failed
*	Remark:
*		Before call AS264_Encode(), this function should be called to create an encoder handle.
**********************************************************/
Int32 ASVE_API ASVE_Create(ASVEncoderParam *pPar, ASVEHANDLE *handle);
typedef Int32 (*pASVECreate)(ASVEncoderParam *pPar, ASVEHANDLE* handle);

/**********************************************************
*	ASVE_Encode
*	Discription:	This function is called to encode one frame or two fields
*	Parameter:		
*		handle[in]:			Encoder handle.
*		pFrame[in][out]:	The pointer to ASVEncoderInSample structure
*	Return:
*		Return 0 if succeed, else a error code with non_zero
*	Remark:
*		Before call this function, AS264_Create() should be called to create a encoder handle.
*	The input I420 data be stored in pFrame->yuv, output h264 bitstream is in pFrame->buffer 
*	with length in pFrame->lValidLength. And  pFrame->lFrameBuffered which gives the frame number buffered in the encoder
*   should be checked: if pFrame->lFrameBuffered > 0 , this function should be called 
*	with pFrame->yuv set equal to zero until pFrame->lFrameBuffered equal to zero, 
*	so that all frames buffered in the encoder could be coded.   
**********************************************************/
Int32 ASVE_API ASVE_Encode(ASVEHANDLE handle, ASVEncoderInSample *pFrame);
typedef Int32 (*pASVEEncode)(ASVEHANDLE handle, ASVEncoderInSample* pFrame);

/**********************************************************
*	ASVE_EndOfStream
*	Discription:	This function is called to notify end of stream to encoder
*	Parameter:		
*		handle[in]:			Encoder handle.
*	Return:
*		Return 0 if succeed, else a error code with non_zero
*	Remark:
*		Before call this function, AS264_Create() should be called to create a encoder handle.
**********************************************************/
Int32 ASVE_API ASVE_EndOfStream(ASVEHANDLE handle);
typedef Int32 (*pASVEEndOfStream)(ASVEHANDLE handle);

/**********************************************************
*	ASVE_Delete
*	Discription:	This function is called to delete encoder handle
*	Parameter:		
*		handle[in]:	Encoder handle
*	Return:
*		Return 0 if succeed, else a error code with non_zero
*	Remark:
*		When encoding finished, do not forget call this function to delete encoder handle.
**********************************************************/
Int32 ASVE_API ASVE_Delete(ASVEHANDLE handle);
typedef Int32 (*pASVEDelete)(ASVEHANDLE handle);

#ifdef __cplusplus
		}
#endif

#endif //__ASVENCODER_INTERFACE_
