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

#ifndef LOGGING_H_
#define LOGGING_H_

#include "error.h"
#include "logging.h"
#include "workthread.h"

#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <chrono>
#include <thread>
#include <cstdio>


namespace ee5
{



//---------------------------------------------------------------------------------------------------------------------
//
//
//
//
struct
    __attribute__ ((visibility ("default")))
//    alignas(size_t)
LogLine
{
    struct Delete { void operator()(LogLine* p) { std::free(p); } };

    typedef std::unique_ptr<LogLine,LogLine::Delete>        LogLinePtr;
    typedef std::chrono::high_resolution_clock::time_point  time_point;
    typedef std::thread::id                                 thread_id;


    static const size_t msg_offset;

    time_point          time;
    const __info*       info;
    thread_id           id;
    size_t              cb_msg;
//    alignas(size_t)
    char                msg[1];

    static RC create_buffer(size_t size,LogLinePtr& pBuffer,size_t* pcMsg = nullptr, char** ppMsg = nullptr)
    {
        char*   pBuf    = nullptr;

        CBREx( size > sizeof(LogLine), e_invalid_argument( 1, "size must be larger than the structure") );

        CMA( pBuf = reinterpret_cast<char*>( std::malloc(size) ) );

        pBuffer.reset( new(pBuf) LogLine() );

        if(pcMsg)
        {
            *pcMsg = size-msg_offset;
        }

        if(ppMsg)
        {
            *ppMsg = &pBuffer->msg[0];
        }

        return s_ok();
    }


};



typedef LogLine::LogLinePtr LogLinePtr;

RC cb_vsnprintf(size_t c,char* p,size_t* pc,char** ppP,const char* fmt,va_list v);


//---------------------------------------------------------------------------------------------------------------------
//
//
//
//
class ConsoleLogger
{
private:
    typedef WorkThread<LogLinePtr>          work_thread;
    typedef std::unique_ptr<work_thread>    ThreadPtr;

    static ThreadPtr pThread;

    static void console_log(const __info* i,...);

    static void Doit(LogLinePtr& pLL)
    {
        time_t now = std::chrono::high_resolution_clock::to_time_t(pLL->time);
        std::chrono::high_resolution_clock::time_point now_s = std::chrono::system_clock::from_time_t(now);

        char buf[100];
        std::strftime(buf, sizeof(buf), "%FT%T", gmtime(&now));
        typedef std::chrono::duration<uint64_t,std::micro>  mi;

        auto fraction = std::chrono::duration_cast<mi>(pLL->time-now_s).count();

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
        printf("[%s.%06luZ]|%s|:%lx %s%s\n",buf, fraction,pLL->info->facility,pLL->id,pLL->info->function,pLL->msg);
#pragma GCC diagnostic pop
    }

protected:
public:
    static int Startup(program_log* pLog)
    {
        pThread.reset( new work_thread( 55, std::function<void(LogLinePtr&)>(ConsoleLogger::Doit) ) );
        *pLog = ConsoleLogger::console_log;
        return pThread->Startup();
    }

    static void Shutdown()
    {
        pThread->Shutdown();
    }

    static int Enqueue(LogLinePtr&& p)
    {
        pThread->Enqueue
        (
            std::move( p ),
            [](LogLinePtr& p)
            {
                p->time = std::chrono::high_resolution_clock::now();
            }
        );

        return s_ok();
    }

};




}       // namespace ee5
#endif  // LOGGING_H_
