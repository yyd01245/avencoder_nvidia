
#ifndef _H_CONSOLE_CODES_


#define _H_CONSOLE_CODES_




#define  _RESET           "\033[0m"   //   reset all attributes to their defaults
#define  _BOLD            "\033[1m"   //   set bold
#define  _HALF_BRIGHT     "\033[2m"   //   set half-bright (simulated with color on a color display)
#define  _UNDER_SCORE     "\033[4m"   //   set underscore (simulated with color on a color  display)
                           //   (the  colors  used  to  simulate dim or underline are set
                           //   using ESC ] ...)
#define  _BLINK           "\033[5m"   //   set blink
#define  _REVERSE_VIDEO   "\033[7m"   //   set reverse video
#define  _0nnnnn          "\033[10m"   //   reset selected mapping, display control flag, and  toggle
                           //   meta flag (ECMA-48 says "primary font").
#define  _1nnnnn          "\033[11m"   //   select null mapping, set display control flag, reset tog‐
                           //   gle meta flag (ECMA-48 says "first alternate font").
#define  _2nnnnn          "\033[12m"   //   select null mapping, set display control flag, set toggle
                           //   meta  flag  (ECMA-48  says "second alternate font").  The
                           //   toggle meta flag causes the high bit of a byte to be tog‐
                           //   gled before the mapping table translation is done.
#define  _3nnnnn          "\033[21m"   //   set normal intensity (ECMA-48 says "doubly underlined")
#define  _4nnnnn          "\033[22m"   //   set normal intensity
#define  _OFF_UNDER_LINE  "\033[24m"   //   underline off
#define  _OFF_BLINK       "\033[25m"   //   blink off
#define  _OFF_REVERSE_VIDEO      "\033[27m"   //   reverse video off
#define  _FG_BLACK      "\033[30m"   //   set black foreground
                        
#define  _FG_RED        "\033[31m"   //   set red foreground
#define  _FG_GREEN      "\033[32m"   //   set green foreground
#define  _FG_BROWN      "\033[33m"   //   set brown foreground
#define  _FG_BLUE       "\033[34m"   //   set blue foreground
#define  _FG_MAGENTA      "\033[35m"   //   set magenta foreground
#define  _FG_CYAN         "\033[36m"   //   set cyan foreground
#define  _FG_WHITE        "\033[37m"   //   set white foreground
#define  _ON_UNDER_SCORE       "\033[38m"   //   set underscore on, set default foreground color
#define  _OFF_UNDER_SCORE      "\033[39m"   //   set underscore off, set default foreground color
#define  _BG_BLACK        "\033[40m"   //   set black background
#define  _BG_RED          "\033[41m"   //   set red background
#define  _BG_GREEN        "\033[42m"   //   set green background
#define  _BG_BROWN        "\033[43m"   //   set brown background
#define  _BG_BLUE         "\033[44m"   //   set blue background
#define  _BG_MAGENTA      "\033[45m"   //   set magenta background
#define  _BG_CYAN         "\033[46m"   //   set cyan background
#define  _BG_WHITE        "\033[47m"   //   set white background
#define  _BG_DEFAULT      "\033[49m"   //   set default background color







#define COL_INFO(str) _FG_GREEN str _RESET
#define COL_WARN(str) _FG_BROWN _BOLD str _RESET
#define COL_ERROR(str)  _FG_RED _BOLD str _RESET

#define COL_REVERSE(str) _REVERSE_VIDEO str _RESET





#endif
