/*
** TinyTrace
** 2108.2.2 16:25
** Virus.V <virusv@live.com>
*/

#ifndef _P_H__
#define _P_H__

#define RED         "\x1B[31m" 
#define GREEN       "\x1B[32m"
#define YELLOW      "\x1B[33m"
#define BLUE        "\x1B[34m"
#define MAGENTA     "\x1B[35m"
#define CYAN        "\x1B[36m"
#define WHITE       "\x1B[37m"

#define RESET       "\x1B[0m"

#define FUNC_COLOR RED
#define TRACE_COLOR GREEN

#define P (fprintf(stderr, "[" FUNC_COLOR "%s" RESET " " TRACE_COLOR "%s:%d" RESET "] => ", __func__, __FILE__, __LINE__), printf)

#endif 

