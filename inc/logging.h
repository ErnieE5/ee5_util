//-------------------------------------------------------------------------------------------------
// Copyright (C) 2014 Ernest R. Ewert
//
// Feel free to use this as you see fit.
// I ask that you keep my name with the code.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//
#pragma once
#include <ee5>

#include "stopwatch.h"

#include <cstdint>
#include <cstddef>
#include <cstdlib>


BNS( ee5 )

#define LOG_FACILITY "KaZa"


struct __info
{
    std::uint64_t   code;
    const char*     function;
    const char*     facility;
    const char*     file;
    const char*     format;
    std::size_t     line;
};


typedef void (*program_log)(__info const *,...);

extern program_log __ee5_log;



#ifdef _MSC_VER
#define FUNCTION_NAME __FUNCTION__
#else
#define FUNCTION_NAME __PRETTY_FUNCTION__
#endif

#define _TRACE_N(funcname,fmt,...) \
    do\
    {\
        static const ee5::__info __info__ = { 0, funcname, LOG_FACILITY, __FILE__," - // " fmt, __LINE__ }; \
        ee5::__ee5_log(&__info__,__VA_ARGS__);\
    } while(0)

#define _TRACE(fmt,...) \
    _TRACE_N( FUNCTION_NAME, fmt, __VA_ARGS__ )

#define LOG_MSG(zone,        fmt,...) _TRACE(fmt,__VA_ARGS__)

#define FRAME_EXIT() \
        static const ee5::__info ___ = { 0, FUNCTION_NAME, LOG_FACILITY, __FILE__, " } // %.6f s", __LINE__ }; \
        struct _                                \
        {                                       \
            s_stopwatch_f s;                    \
            ~_()                                \
            {                                   \
                ee5::__ee5_log(&___,s.delta()); \
            }                                   \
        } __                                    \


#define LOG_FRAME(zone,fmt,...) \
        FRAME_EXIT(); \
        do\
        {                   \
            static const ee5::__info __info__ = { 0, FUNCTION_NAME, LOG_FACILITY, __FILE__," { // " fmt, __LINE__ }; \
            ee5::__ee5_log(&__info__,__VA_ARGS__);\
        } while(0)


#define LOG_UNAME(  funcname,       fmt,...) _TRACE_N(funcname,fmt,__VA_ARGS__)
#define LOG_ENTRY(  zone,           fmt,...)
#define LOG_BINARY( zone,cb,ptr,    fmt,...)
#define LOG_STREAM( zone,s,         fmt,...)
#define LOG_ERROR(                  fmt,...) _TRACE(fmt,__VA_ARGS__)
#define LOG_ALWAYS(                 fmt,...) _TRACE(fmt,__VA_ARGS__)
#define LOG_DEBUG(  zone,           fmt,...)

ENS( ee5 )
