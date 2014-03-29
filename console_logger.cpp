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


namespace ee5
{

const size_t LogLine::msg_offset = offsetof(LogLine,msg);

ConsoleLogger::ThreadPtr ConsoleLogger::pThread;



//---------------------------------------------------------------------------------------------------------------------
//
//
//
//
EE5RC cb_vsnprintf(size_t c,char* p,size_t* pc,char** ppP,const char* fmt,va_list v)
{
    int i = vsnprintf(p,c,fmt,v);

    i = i + 0;

    return s_ok();
}



//---------------------------------------------------------------------------------------------------------------------
//
//
//
//
void ConsoleLogger::console_log(const __info* i,...)
{
    EE5RC   rc  = s_ok();
    size_t  c   = 0;
    char*   p   = nullptr;

    va_list arg_list;
    va_start(arg_list, i);

    LogLinePtr pLogLine;

    rc = LogLine::create_buffer( 4096, pLogLine,&c, &p );

    if( rc == s_ok() )
    {
        pLogLine->id    = std::this_thread::get_id();
        pLogLine->info  = i;

        rc = cb_vsnprintf(c, p, &c, &p, i->format, arg_list );

        if( rc == s_ok() )
        {
            rc = Enqueue( std::move(pLogLine) );
        }
    }
}



}
