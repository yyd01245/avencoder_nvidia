#ifndef _H_BVBCS_

#define _H_BVBCS_


#define SHMHDR_CLEAR 0
#define SHMHDR_DIRTY 1


struct _shm_head{
    int count;//count after last clear oper
    int flag;//should never access directly 
    int x;
    int y;//top left
    int x1;
    int y1;//bottom right
    int w;
    int h;
    
};

typedef struct _shm_head bvbcs_shmhdr;

static inline void shmhdr_clear(bvbcs_shmhdr*hdr){
    hdr->flag = SHMHDR_CLEAR;
    hdr->count=0;
    hdr->x  =hdr->y  =0;
    hdr->x1 =hdr->y1 =0;
    hdr->w  =hdr->h  =0;

}


static inline int shmhdr_get_extents(bvbcs_shmhdr*hdr,int*x,int*y,int*w,int*h,int*x1,int*y1){

    //LOCK
    if(hdr->count==0)
        return 0;

    if(x)
        *x=hdr->x;
    if(y)
        *y=hdr->y;
    if(x1)
        *x1=hdr->x1;
    if(y1)
        *y1=hdr->y1;

    if(w)
        *w=hdr->w;
    if(h)
        *h=hdr->h;
    //UNLOCK

    shmhdr_clear(hdr);

    return 1;
}








#endif //_H_BVBCS_
