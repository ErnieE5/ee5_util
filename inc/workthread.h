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

#include <delegate.h>
#include <spin_locking.h>

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
template<typename L,typename F,typename...TArgs>
auto framed_lock(L& _mtx,F _f,TArgs...args) -> decltype(_f(args...))
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
        frame_lock _lock(mtx);
        cv.wait( _lock, [&]{ return event_set; } );
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
// load is the max number of items to pull from the queue at a given time.
//
template<typename QItem,size_t load = 100>
class WorkThread
{
private:
    using mutex         = spin_mutex; // std::mutex;
    using thread_method = object_method_delegate<WorkThread,void>;
    using frame_lock    = std::lock_guard<mutex>;
    using work_queue    = std::queue<QItem>;
    using work_method   = std::function<void(QItem&)>;
    using work_array    = std::array<QItem,load>;

    ThreadEvent         sig;
    size_t              user_id;
    std::atomic_size_t  pending;
    std::thread         thread;
    work_method         method;

    mutex               data_lock;
    work_queue          queue;
    bool                quit    = false;
    bool                abandon = false;

    void Thread()
    {
        work_array  pending_work;
        bool        running = true;

        // The code inside this captured lambda needs to operate while
        // under the data_lock. It is unclear if this code is more efficient
        // or less efficient. Profiling the code didn't expose any major
        // issues in an optimized build. The code can be written a number
        // of ways and it is unclear (to me) at this point if this is
        // faster / slower or less / more understandable.
        //
        // Originally this was an "in-line" lambda and could be moved
        // back to that style.
        //
        auto populate_work_array = [&,this]()->size_t
        {
            // The number of items we are going to take from the queue
            // and place in the pending_work array.
            //
            size_t item_count = std::min( queue.size(), load );

            // Move items from the queue of items into the pending
            // items. Items that are in the pending_work array can
            // not be abandoned.
            //
            for( size_t i = 0; i < item_count ; ++i, queue.pop() )
            {
                pending_work[i] = std::move( queue.front() );
            }

            running = !quit;
            pending = item_count;

            return item_count;
        };

        // The main thread item processing loop
        //
        while( running )
        {
            // The side effect of this framed_lock is that running, queue,
            // and pending can all be modified. The frame lock keeps other
            // threads from fiddling with the values.
            //
            size_t items = framed_lock( data_lock, populate_work_array );

            if( items > 0 )
            {
                // Run each of the work items.
                //
                for( size_t i = 0 ; i < items ; ++i, --pending )
                {
                    // Transfer ownership of the transfer buffer to the
                    // temporary so that the memory gets released at the
                    // end of the frame.
                    //
                    QItem arg( std::move( pending_work[i] ) );

                    // Do the work
                    //
                    method( arg );
                }
            }
            else if( running )
            {
                // Stall the thread until a signal wakes us up
                //
                sig.Chill();
            }
        }

        // When the user terminates the thread asking for a join,
        // we don't abandon the work. This loop finishes any left
        // over queued work before we exit.
        //
        if( !abandon )
        {
            while( queue.size() > 0 )
            {
                method( queue.front() );
                queue.pop();
            }
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

    RC Startup()
    {
        thread = std::thread( thread_method( this, &WorkThread::Thread ) );
        return s_ok();
    }

    void Shutdown()
    {
        Quit();
    }

    size_t Pending()
    {
        return framed_lock( data_lock, [&]()->size_t { return queue.size() + pending; } );
    }

    void Quit(bool join = true)
    {
        framed_lock( data_lock, [&,this]
        {
            quit    = true;
            abandon = !join;
        });

        // Wake up the thread.
        sig.Set();

        if( join )
        {
            thread.join();
        }
    }

    bool Enqueue( QItem&& p )
    {
        bool enqueued = framed_lock( data_lock, [&]()->bool
        {
            if(!quit)
            {
                queue.push( std::move( p ) );
            }

            return !quit;
        });

        // Wake up the thread if it was sleeping
        //
        if( enqueued )
        {
            sig.Set();
        }

        return enqueued;
    }
};



}       // namespace ee5
#endif  // EE5_WORKTHREAD_H_