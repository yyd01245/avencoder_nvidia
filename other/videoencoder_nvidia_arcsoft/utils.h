
#ifndef _H_UTILS_
#define _H_UTILS_

#include<unistd.h>
#include<fcntl.h>
#include<sys/types.h>
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<errno.h>

int get_id(int range);


typedef struct _loadavg{

//    unsigned short gval[4];
    int nb_cores;
    //unsigned short *gvals;
    unsigned short gvals[8];


}loadavg;

int get_gpu_loadavg(loadavg*l);

int get_next_gpuid();


#endif
