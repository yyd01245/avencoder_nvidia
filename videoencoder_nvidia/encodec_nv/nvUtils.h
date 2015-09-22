//
// Copyright 1993-2014 NVIDIA Corporation.  All rights reserved.
//
// Please refer to the NVIDIA end user license agreement (EULA) associated
// with this source code for terms and conditions that govern your use of
// this software. Any use, reproduction, disclosure, or distribution of
// this software and related documentation outside the terms of the EULA
// is strictly prohibited.
//
////////////////////////////////////////////////////////////////////////////

#ifndef NVUTILS_H
#define NVUTILS_H

#include <stdio.h>
#include <sys/time.h>
#include <limits.h>

#define FALSE 0
#define TRUE  1
#define INFINITE UINT_MAX
#define stricmp strcasecmp
#define FILE_BEGIN               SEEK_SET
#define INVALID_SET_FILE_POINTER (-1)
#define INVALID_HANDLE_VALUE     ((void *)(-1))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

typedef void* HANDLE;
typedef void* HINSTANCE;
typedef unsigned long DWORD, *LPWORD;
typedef DWORD FILE_SIZE;


#define SEC_TO_NANO_ULL(sec)    ((unsigned long long)sec * 1000000000)
#define MICRO_TO_NANO_ULL(sec)  ((unsigned long long)sec * 1000)



__inline bool operator==(const GUID &guid1, const GUID &guid2)
{
     if (guid1.Data1    == guid2.Data1 &&
         guid1.Data2    == guid2.Data2 &&
         guid1.Data3    == guid2.Data3 &&
         guid1.Data4[0] == guid2.Data4[0] &&
         guid1.Data4[1] == guid2.Data4[1] &&
         guid1.Data4[2] == guid2.Data4[2] &&
         guid1.Data4[3] == guid2.Data4[3] &&
         guid1.Data4[4] == guid2.Data4[4] &&
         guid1.Data4[5] == guid2.Data4[5] &&
         guid1.Data4[6] == guid2.Data4[6] &&
         guid1.Data4[7] == guid2.Data4[7])
    {
        return true;
    }

    return false;
}


__inline bool operator!=(const GUID &guid1, const GUID &guid2)
{
    return !(guid1 == guid2);
}


#define PRINTERR(f,args...) \
    fprintf(stderr,f,##args);


#endif


