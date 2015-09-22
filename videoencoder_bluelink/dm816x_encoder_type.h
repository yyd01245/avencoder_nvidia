/*!
\file

共用的结构放这里，减少耦合
*/


#ifndef DM816X_ENCODER_TYPE_H
#define DM816X_ENCODER_TYPE_H

#define DM_DEV_ANY -1
#define DM_CHAN_ANY -1

typedef struct{
	int width;
	int height;
	int dev_id;  /*DM_DEV_ANY */
	int chan_id; /*DM_CHAN_ANY */
	int codec;/*TODO ,only h264 now  0:h264 1:mpeg2*/

	int gop_size;
	int bitrate;

	int debug_modify_sps;
	int debug_add_sps;
}dm816x_venc_params_t;

typedef struct{
	int active_x1;
	int active_y1;
	int active_x2;
	int active_y2;
	int force_key;
}dm816x_venc_data_t;


typedef struct{
	int is_key;
	int bits_size;
	int bits_offset;/* bit流存储位置可能有一个偏移 */
	
}dm816x_venc_bits_t;

typedef struct{
	dm816x_venc_data_t data;
	dm816x_venc_bits_t bits;
}dm816x_venc_frame_t;

typedef struct{
	int sps_pps_size;
} dm816x_sps_pps_t;


typedef struct
{
  unsigned long encode_frame_count;
  unsigned long skip_frame_count;
  int  encode_avg_time;
}dm816x_frame_info,*pdm816x_frame_info;
#endif


