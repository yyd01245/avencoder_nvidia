
#ifndef _H_FORMATS_
#define _H_FORMATS_






void load_yuv_yv12(unsigned char*yv12,unsigned char*nv12,int width,int height,int sstride,int dstride);

void load_yuv_nv12(unsigned char*inyuv, unsigned char*outyuv,int width,int height,int istride,int ostride);




void load_rgb_bgrx(
        unsigned char*bgrx,
        unsigned char*nv12, 
        int pleft,int ptop,int pwidth,int pheight,//rgb patch rect
        int width,int height,//rgb data size
        int sstride,
        int dstride //yuv data stride<pixel>
        );


void load_rgb_bgrx_(unsigned char*yuv,unsigned char*rgb,
        int left,int top,int width,int height,//patch rectangle
        int rgbheight,
        int ostride);


void load_rgb_bgrx_2(unsigned char*yuv,unsigned char*rgb,
        int left,int top,int width,int height,//patch rectangle
        int rgbheight,
        int ostride);


// CUDA Conversion Routines. 

void load_rgb_bgrx_cuda(
        unsigned char* oyuv,/*device*/
        unsigned char* devrgb,/*device */
        unsigned char* rgb, /*input data host*/
        int left,int top,int width,int height,//rgb patch rect
        int rgbwidth,int rgbheight,//rgb data size
        int ostride //yuv data stride
        );





void load_yuv_yv12_cuda(
        unsigned char* oyuv,/*device*/
        unsigned char* devyv12,/*device */
        unsigned char*iyuv, /*input data host*/
        int width,int height,/*real size*/
        int istride,int ostride
        );


#endif
