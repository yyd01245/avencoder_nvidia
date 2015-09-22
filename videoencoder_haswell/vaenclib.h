#include "buffdef.h"

int init_encoder(int argc, char *argv[]);
int encode_frame(unsigned char *inputdata,
			   struct coded_buff *avc_p);
int close_encoder();
