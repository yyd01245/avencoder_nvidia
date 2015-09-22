
#ifndef _H_CHECK_CALL_
#define _H_CHECK_CALL_


#include"colors.h"
#include<stdio.h>
#include<string.h>
#include<errno.h>



#define OUTPUT_FMT0  "Nooo!! "  "((" _FG_CYAN _UNDER_SCORE _BOLD"%s"_RESET "))<< "_FG_BROWN "%s"_RESET ":\n"

#define OUTPUT_FMT1  "Nooo!! "  "((" _FG_CYAN _UNDER_SCORE _BOLD"%s"_RESET "))<< "_FG_BROWN "%s"_RESET ":{"_FG_RED _BOLD"%s" _RESET "}\n"

//#define OUTPUT_FMT2  "Nooo!! "  "((" _FG_CYAN _UNDER_SCORE _BOLD"%s"_RESET "))<< "_FG_BROWN "%s"_RESET ":{"_FG_RED _BOLD"%s" _RESET "}\n"





#define CHK_EXPRe(expr,msg) if(expr){\
    fprintf(stderr,OUTPUT_FMT1,#expr ,msg,strerror(errno));\

#define END_CHK_EXPRe }



#define CHK_EXPR(expr,msg) if(expr){\
    fprintf(stderr,OUTPUT_FMT0,#expr ,msg);\

#define END_CHK_EXPR }


#define CHK_EXPR_ONLY(expr,msg) if(expr) fprintf(stderr,OUTPUT_FMT0,#expr ,msg);

#define CHK_EXPRe_ONLY(expr,msg) if(expr) fprintf(stderr,OUTPUT_FMT1,#expr ,msg,strerror(errno));


#define CHK_RUNe(expr,msg,stmt) if(expr){\
    fprintf(stderr,OUTPUT_FMT1,#expr ,msg,strerror(errno));\
    stmt;}


#define CHK_RUN(expr,msg,stmt) if(expr){\
    fprintf(stderr,OUTPUT_FMT0,#expr ,msg);\
    stmt;}



#define CHK_EXIT(expr,msg) CHK_RUN(expr,msg,exit(-1))

//#define CHK_EXITe(expr,msg) CHK_RUNe(expr,msg,exit(-2))



#define CHK_ABORT(expr,msg) CHK_RUN(expr,msg,abort())

//#define CHK_ABORTe(expr,msg) CHK_RUNe(expr,msg,abort())







#endif
