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

#include <error.h>
#include <logging.h>
#include <workthread.h>
#include <static_memory_pool.h>

#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <chrono>
#include <thread>
#include <cstdio>

BNS( ee5 )

//---------------------------------------------------------------------------------------------------------------------
//
//
//
//
struct
//    __attribute__ ((visibility ("default")))
//    alignas(size_t)
LogLine
{
    using time_point    = std::chrono::high_resolution_clock::time_point;
    using thread_id     = std::thread::id;
    using mem_pool_t    = static_memory_pool<2048,1000>;
    using log_line_ptr  = mem_pool_t::unique_type<LogLine>;

    static const size_t msg_offset;
    static mem_pool_t   mem;

    time_point          time;
    const __info*       info;
    thread_id           id;
    size_t              cb_msg;
    char                msg[1];

    static RC create_buffer(size_t size,log_line_ptr& pBuffer,size_t* pcMsg = nullptr, char** ppMsg = nullptr)
    {
        CBREx( size <= mem_pool_t::max_item_size, e_invalid_argument( 1, "size must be larger than the structure") );

        do
        {
            pBuffer = mem.acquire_unique<LogLine>( );
        }
        while(!pBuffer);


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



typedef /*typename*/ LogLine::log_line_ptr LogLinePtr;

RC cb_vsnprintf(size_t c,char* p,size_t* pc,char** ppP,const char* fmt,va_list v);


//---------------------------------------------------------------------------------------------------------------------
//
//
//
//
namespace c = std::chrono;
class ConsoleLogger
{
private:
    using hrc_t         = c::high_resolution_clock;
    using sys_t         = c::system_clock;
    using time_point    = c::high_resolution_clock::time_point;
    using thread_t      = WorkThread<LogLinePtr>;
    using thread_ptr    = std::unique_ptr<thread_t>;
    using milli         = c::duration<uint64_t, std::milli>;
    using micro         = c::duration<uint64_t, std::micro>;
    static thread_ptr pThread;

    static void console_log(const __info* i,...);

    static void Doit(LogLinePtr& pLL)
    {
        // Convert the HRC time captured to time_t system clock representation
        // then, convert it back. This will give use a value without the
        // micro/milliseconds as the system clock truncates the value to seconds.
        // The duration between the two is the fractional part which gets
        // converted to the millisecond or microsecond value for the time stamp.
        //
        time_t          now     = hrc_t::to_time_t(pLL->time);
        time_point      now_s   = sys_t::from_time_t(now);
        hrc_t::duration d       = pLL->time - now_s;

        char buf[100];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", std::gmtime(&now));

#ifdef _MSC_VER
        auto  fraction  = c::duration_cast<milli>(d).count();
        printf("[%s.%03luZ]|%s|:%lx %s %s\n", buf, fraction, pLL->info->facility, pLL->id, pLL->info->function, pLL->msg);
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
        auto  fraction  = c::duration_cast<micro>(d).count();
        printf("[%s.%06luZ]|%s|:%lx %s %s\n",buf, fraction,pLL->info->facility,pLL->id,pLL->info->function,pLL->msg);
#pragma GCC diagnostic pop
#endif
    }

protected:
public:
    static RC Startup(program_log* pLog)
    {
        pThread.reset( new thread_t( 55, std::function<void(LogLinePtr&)>(ConsoleLogger::Doit) ) );
        *pLog = ConsoleLogger::console_log;
        return pThread->Startup();
    }

    static void Shutdown()
    {
        pThread->Shutdown();
    }

    static RC Enqueue(LogLinePtr&& p)
    {
        pThread->Enqueue( std::forward<LogLinePtr>(p) );

        return s_ok();
    }

};




ENS( ee5 )
