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
#include <queue>
#include <thread>


namespace ee5
{



//---------------------------------------------------------------------------------------------------------------------
//
//
//
//
template<typename F,typename L = std::mutex>
void framed_lock(L& _mtx,F _f)
{
    std::unique_lock<L> __lock(_mtx);
    _f();
}



//---------------------------------------------------------------------------------------------------------------------
//
//
//
//
class ThreadEvent
{
private:
    typedef std::unique_lock<std::mutex>    frame_lock;

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

            Process(work_to_do);
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
