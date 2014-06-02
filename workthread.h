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

#ifndef EE5_WORKTHREAD_H_
#define EE5_WORKTHREAD_H_

#include "delegate.h"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <atomic>
#include <queue>
#include <thread>
#include <cassert>


namespace ee5
{

    //---------------------------------------------------------------------------------------------------------------------
    //
    //
    //
    //
    class Timer
    {
    private:
        typedef std::chrono::time_point<std::chrono::high_resolution_clock> hrc_time_point;
        hrc_time_point s;

    protected:

    public:
        Timer()
        {
            s = std::chrono::high_resolution_clock::now();
        }

        template< typename D = std::chrono::duration< float, std::micro > >
        D Delta()
        {
            return std::chrono::high_resolution_clock::now() - s;
        }

//         template<typename T = std::ostream,typename M = std::chrono::duration<float> >
//         void PrintDelta(T& o = std::cout)
//         {
//             //        std::streamsize p = std::cout.precision(4);
//             //        o << std::chrono::duration_cast<M>(Delta()).count() << std::endl;
//             //        std::cout.precision(p);
//         }
    };



//---------------------------------------------------------------------------------------------------------------------
//
//
//
//
class spin_barrier
{
    std::atomic_flag barrier = ATOMIC_FLAG_INIT;

public:
    spin_barrier() {}
    spin_barrier(const spin_barrier&) = delete;

    void lock()
    {
        while( barrier.test_and_set( std::memory_order_acquire ) );
    }

    void unlock()
    {
        barrier.clear( std::memory_order_release );
    }

};

class spin_mutex
{
    static constexpr std::memory_order release = std::memory_order_release;
    static constexpr std::memory_order relaxed = std::memory_order_relaxed;
    static constexpr std::memory_order acquire = std::memory_order_acquire;

    std::atomic_bool barrier;

public:
    spin_mutex()
    {
    }

    void lock()
    {
        bool expected = false;
        while( barrier.compare_exchange_weak( expected, true, acquire, relaxed ))
        {
            expected = false;
        }
    }

    void unlock()
    {
        bool expected = true;
        while( ! barrier.compare_exchange_strong( expected, false, release ) )
        {
            // You are breaking the contract if this assert fires!
            assert( false );
            expected = true;
        }
    }

    bool try_lock()
    {
        bool expected = false;
        return barrier.compare_exchange_strong( expected, true, acquire, relaxed );
    }

};



//---------------------------------------------------------------------------------------------------------------------
//
//
//
//
template<typename L = spin_barrier,typename F = void (*)(),typename...TArgs>
void framed_lock(L& _mtx,F _f,TArgs...args)
{
    std::lock_guard<L> __lock(_mtx);
    return _f(args...);
}



//---------------------------------------------------------------------------------------------------------------------
//
//
//
//
class ThreadEvent
{
private:
    using frame_lock = std::unique_lock<std::mutex>;

    std::mutex              mtx;
    std::condition_variable cv;
    bool                    event_set;

protected:
public:
    ThreadEvent()
    {
        event_set = false;
    }
    ~ThreadEvent()
    {
    }

    void Chill(bool after = false)
    {
        std::this_thread::yield();
        frame_lock _lock(mtx);
        cv.wait( _lock, [this]{ return event_set; } );
        event_set = after;
    }

    void Set()
    {
        framed_lock( mtx, [&] { event_set = true; } );
        cv.notify_one();
    }

    void Reset()
    {
        framed_lock( mtx, [&] { event_set = false; } );
    }
};

#include <unistd.h>


//---------------------------------------------------------------------------------------------------------------------
//
//
//
//
template<typename QItem>
class WorkThread
{
private:
    typedef object_method_delegate<WorkThread,void> thread_method;
    typedef std::unique_lock<std::mutex>            frame_lock;
    typedef std::queue<QItem>                       work_queue;
    typedef std::function<void(QItem&)>             work_method;

    ThreadEvent     sig;
    size_t          user_id;
    std::thread     thread;
    work_method     method;
    std::mutex      data_lock;
    work_queue      queue;
    bool            quit    = false;
    bool            abandon = false;

    void Process(work_queue& items)
    {
        while( items.size() > 0 )
        {
            method( items.front() );
            items.pop();
            //std::this_thread::yield();
        }
    }

    void Thread()
    {
        while( !quit )
        {
            sig.Chill();

            work_queue work_to_do;

            framed_lock( data_lock, [&]
            {
                while( queue.size() )
                {
                    work_to_do.push( std::move( queue.front() ) );
                    queue.pop();
                }
            } );


            {
                Timer taft;
                size_t s = work_to_do.size();
                Process( work_to_do );
                using mil = std::chrono::milliseconds;
                auto milli = std::chrono::duration_cast<mil>(taft.Delta()).count();
                if(milli > 1000)
                {
                    LOG_UNAME("T", "%lu : %6lu/%6lu : %3li ms",user_id,s,queue.size(),milli );
                }
            }
        }



        if( !abandon && queue.size() )
        {
            Process(queue);
        }
    }


public:
    WorkThread(size_t _user,work_method _f) : user_id(_user),method(_f)
    {
    }
    WorkThread(WorkThread&& _o) : user_id(_o.user_id),method( std::move( _o.method )  )
    {
    }
    ~WorkThread()
    {
    }

    int_fast64_t Startup()
    {
        thread = std::thread( thread_method( this, &WorkThread::Thread ) );

        return 0;
    }

    void Shutdown()
    {
        Quit();
    }

    size_t Pending()
    {
        frame_lock _lock(data_lock);
        return queue.size();
    }

    void Quit(bool join = true)
    {
        {
            frame_lock _lock(data_lock);
            quit = true;
        }

        sig.Set();

        if( join )
        {
            thread.join();
        }
        else
        {
            abandon = true;
        }
    }

    void Enqueue( QItem&& p, std::function<void(QItem&)> f /* = nullptr*/ )
    {
        bool signal = false;
        {
            frame_lock _lock(data_lock);

            signal = !quit;

            if(signal)
            {
                if( f )
                {
                    f(p);
                }
                queue.push( std::move( p ) );
            }
        }

        if( signal )
        {
            sig.Set();
        }
    }
};



}       // namespace ee5
#endif  // EE5_WORKTHREAD_H_
