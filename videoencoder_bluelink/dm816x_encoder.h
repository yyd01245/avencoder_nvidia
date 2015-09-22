
/*!
\file
*/

#ifndef DM816X_ENCODER_H
#define DM816X_ENCODER_H

typedef unsigned long dm_venc_handle_t;

#include "dm816x_encoder_type.h"

typedef struct{
	unsigned char*data0;
	unsigned char*data1;
	int pitch0;
	int pitch1;
	int max_height;
	unsigned char*bits;
}dm_venc_buff_info_t;

int dm_venc_open(dm_venc_handle_t*handle);
int dm_venc_close(dm_venc_handle_t handle);
int dm_venc_get_fileno(dm_venc_handle_t handle);
int dm_venc_get_dev_num(dm_venc_handle_t handle);
int dm_venc_get_dev_id(dm_venc_handle_t handle);
int dm_venc_get_chan_id(dm_venc_handle_t handle);
int dm_venc_start_channel(dm_venc_handle_t handle,dm816x_venc_params_t*venc_params);
int dm_venc_encode_frame(dm_venc_handle_t handle,dm816x_venc_frame_t*frame);


int dm_venc_is_write_idle(dm_venc_handle_t handle);
int dm_venc_is_enc_idle(dm_venc_handle_t handle);
/*! \brief 可以做异步接口
*/
int dm_venc_write_data(dm_venc_handle_t handle,dm816x_venc_data_t *data);
int dm_venc_read_bits(dm_venc_handle_t handle,dm816x_venc_bits_t*bits);

int dm_venc_get_buff_info(dm_venc_handle_t handle,dm_venc_buff_info_t*buff_info);
int dm_venc_get_resource_using(dm_venc_handle_t handle,dm816x_frame_info* puse);
int dm_venc_change_encode_param(dm_venc_handle_t handle, dm816x_venc_params_t*venc_params);


/*!
\example
   int fd = open("/dev/dm816x_encoder",O_RDWR);
   int dev_num = ioctl(fd,DM816x_IOC_DEV_NUM,0);
   dm816x_venc_params_t params;
   params.width = 720;
   params.height = 576;
   params.dev_id = 0;
   params.codec = 0;

   int pitch;

   //USE NV12 ,YUV420SP ,not YUV420P , dm816x pitch need aglin 16.
   pitch = ((params.width + 15) /16) * 16;

   params.pitch = pitch;

   ioctl(fd,DM816x_IOC_VENC_START,&params);

   int frame_size = pitch * params.width + pitch*params.width/2;
   unsigned char *buf =(unsigned char*)malloc(frame_size);


   unsigned char outbuf[1024*1024];

   dm816x_venc_frame_t frame;
   frame.in_data = buf;
   frame.in_len = frame_size;
   frame.out_data = outbuf;
   frame.out_size = sizeof(outbuf);
   //will block
   ioctl(fd,DM816x_IOC_ENCODER_FRAME,&frame);


*/

#endif

