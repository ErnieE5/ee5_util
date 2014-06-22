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
#include "console_logger.h"
#include "workthread.h"
#include <cstdarg>

BNS( ee5 )

const size_t LogLine::msg_offset = offsetof(LogLine,msg);

ConsoleLogger::thread_ptr ConsoleLogger::pThread;

//---------------------------------------------------------------------------------------------------------------------
//
//
//
//
RC cb_vsnprintf(size_t c,char* p,size_t* pc,char** ppP,const char* fmt,va_list v)
{
#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif
    int i = std::vsnprintf(p,c,fmt,v);
#ifdef _MSC_VER
#pragma warning(default: 4996)
#endif
    if( i < 0 || static_cast<size_t>(i) > c )
    {
        return e_overflow();
    }

    *pc  -= i;
    *ppP += i;

    return s_ok();
}

LogLine::mem_pool_t LogLine::mem;

//---------------------------------------------------------------------------------------------------------------------
//
//
//
//
size_t work_thread_id();
void ConsoleLogger::console_log(const __info* i,...)
{
    RC      rc  = s_ok();
    size_t  c   = 0;
    char*   p   = nullptr;

    va_list arg_list;
    va_start(arg_list, i);

    LogLinePtr pLogLine;

    rc = LogLine::create_buffer( LogLine::mem_pool_t::max_item_size, pLogLine,&c, &p );

    if( rc == s_ok() )
    {
        pLogLine->time  = hrc_t::now();
        pLogLine->id    = work_thread_id();
        pLogLine->info  = i;

        rc = cb_vsnprintf(c, p, &c, &p, i->format, arg_list );

        if( rc == s_ok() )
        {
            rc = Enqueue( std::move(pLogLine) );
        }
    }
}



ENS( ee5 )
