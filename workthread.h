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
        barrier = false;
    }

    void lock()
    {
        bool expected = false;
        while( ! barrier.compare_exchange_strong( expected, true, acquire, relaxed ) )
        {
            expected = false;
        }
    }

    void unlock()
    {
         assert( barrier.load() == true );
         barrier.store(false,release);
    }

    bool try_lock()
    {
        bool expected = false;
        return barrier.compare_exchange_strong( expected, true, acquire, relaxed );
    }

};


struct is_64_bit
{
    static const bool value = sizeof(void*) == sizeof(uint64_t);

    using  value_type   = uint_fast64_t;
    using  barrier_type = std::atomic_uint_fast64_t;


    static constexpr value_type addend_exclusive    = 0x0000100000000000;
    static constexpr value_type mask_exclusive      = 0x0FFFF00000000000;
    static constexpr value_type addend_shared       = 0x0000000000000010;
    static constexpr value_type mask_shared         = 0x000000FFFFFFFFF0;
};

struct is_32_bit
{
//    static const bool value = sizeof(void*) == sizeof(uint32_t);
//
//     using  value_type   = uint_fast32_t;
//     using  barrier_type = std::atomic_uint_fast32_t;
//
//     static constexpr value_type exclusive_addend    = 0x00100000;
//     static constexpr value_type maximum_exclusive   = 0xFFF00000;
//     static constexpr value_type maximum_shared      = 0x0000FFFF;
};


template<typename storage_traits = typename std::conditional<is_64_bit::value,is_64_bit,is_32_bit>::type >
class spin_shared_mutex
{
private:
    static constexpr std::memory_order release = std::memory_order_release;
    static constexpr std::memory_order relaxed = std::memory_order_relaxed;
    static constexpr std::memory_order acquire = std::memory_order_acquire;

    using storage_t = typename storage_traits::value_type;
    using barrier_t = typename storage_traits::barrier_type;

    static constexpr storage_t addend_exclusive = storage_traits::addend_exclusive;
    static constexpr storage_t mask_exclusive   = storage_traits::mask_exclusive;
    static constexpr storage_t addend_shared    = storage_traits::addend_shared;
    static constexpr storage_t mask_shared      = storage_traits::mask_shared;

    barrier_t  barrier;

public:
    spin_shared_mutex()
    {
        barrier = 0;
    }

    // Notes on the write lock. Th
    void lock()
    {
        // Spin until we acquire the write lock. Which we know is "ours" because
        // no one else owned it before.
        //
        storage_t ex;
        while( ( ex = barrier.fetch_add(addend_exclusive,release) & mask_exclusive ) > 0 )
        {
            barrier.fetch_sub(addend_exclusive);

            for(;ex&mask_exclusive;ex -= addend_exclusive)
            {
                std::this_thread::yield();
            }
        }

        // Spin while any readers are still owning a lock
        //
        while( ( barrier.load() & mask_shared ) > 0 );
    }

    void unlock()
    {
        barrier.fetch_sub(addend_exclusive);
    }

    bool try_lock()
    {
        // The only way this can succeed is if ALL writers and all readers
        // are not doing anything, which means that the value in the barrier
        // had to be zero.
        //
        bool owned = barrier.fetch_add(addend_exclusive) == 0;

        if( !owned )
        {
            barrier.fetch_sub(addend_exclusive);
        }

        return owned;
    }

    void lock_shared()
    {
        // So "take" the lock,
        //
        if( ++barrier & mask_exclusive )
        {
            // but spin until a "max read" slot is available OR while a writer
            // has the lock OR is waiting for readers to finish.
            //
            while( barrier.load(acquire) & mask_exclusive );
        }
    }

    void unlock_shared()
    {
        barrier.fetch_sub(addend_shared);
    }

    bool try_lock_shared()
    {
        barrier.fetch_add(addend_shared);

        bool owned = ( ( barrier.load() & mask_shared ) <= mask_shared );

        if(!owned)
        {
            barrier.fetch_sub(addend_shared);
        }

        return owned;
    }

};



//---------------------------------------------------------------------------------------------------------------------
//
//
//
//
template<typename L,typename F,typename...TArgs>
void framed_lock(L& _mtx,F _f,TArgs...args)
{
    std::lock_guard<L> __lock(_mtx);
    //std::unique_lock<L> __lock(_mtx);
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
    using mutex = spin_mutex; //    std::mutex;
    typedef object_method_delegate<WorkThread,void> thread_method;
    typedef std::unique_lock<mutex>                 frame_lock;
    typedef std::queue<QItem>                       work_queue;
    typedef std::function<void(QItem&)>             work_method;

    ThreadEvent     sig;
    size_t          user_id;
    std::thread     thread;
    work_method     method;
    mutex           data_lock;
    work_queue      queue;
    bool            quit    = false;
    bool            abandon = false;
    std::atomic_size_t pending;


    void Process(work_queue& items)
    {
        while( items.size() > 0 )
        {
            method( items.front() );
            items.pop();
            --pending;
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

                pending += work_to_do.size();

            } );

            Process( work_to_do );
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

//        printf("hh %lu\n",user_id);

        return 0;
    }

    void Shutdown()
    {
        Quit();
    }

    size_t Pending()
    {
        frame_lock _lock(data_lock);
        return queue.size() + pending;
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

    void Enqueue( QItem&& p, std::function<void(QItem&)> f  = nullptr )
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
