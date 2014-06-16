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

#include <condition_variable>
#include <memory>
#include <mutex>
#include <atomic>
#include <queue>
#include <thread>
#include <cassert>

//
// Note: 
//
//  On virtual hardware (pretty much every cloud system in existence today), a spin lock can 
//  end up being MORE costly than a typical mutex (or CriticalSection). Choose wisely and 
//  always TEST with time on target before making a choice.
//
//  The statement that a spin lock is ALWAYS faster than a mutex is simply NOT TRUE when ALL 
//  dependent factors are accounted for.
//
namespace ee5
{

//-------------------------------------------------------------------------------------------------
// spinning barriers
//
//  While doing parallel coding using a spin barrier ~can~ be more efficient over a relatively 
//  long time. Much of any gain is largely dependent on the load factor. If your program is well
//  split up and the scheduled operations are very short duration, the "win" can be as much as 7%.
//  When I say ~can~ it is just that. The win is over the VERY long run and is an average. Lots of 
//  things get in the way if you are running on a preemptively scheduled OS. The gain over a mutex
//  is largely due to not incurring a call into the kernel to potentially park the thread. 
//
//  The benefit of a spin lock is considerably less when a "larger" amount of work per scheduled
//  work item is done. Anytime a work thread is preempted and the OS grabs the quantum during a 
//  spin, the spike in time obliterates any benefit.
//
//      for(size_t q = 0;q < 200;q++)
//      {
//          ttf += ThreadpoolTest::Factorial(q*2);
//      }
//
//    rlt St5mutex                       39.47361948 m  80000000 /  0: 10000579  1: 10008047  2: 10003251  3:  9998440  4:  9994538  5:  9997243  6:  9996347  7: 10001555 
//    rlt N3ee510spin_posixE             39.44335763 m  80000000 /  0:  9998613  1: 10000433  2:  9998422  3: 10008128  4:  9997107  5: 10000958  6:  9999421  7:  9996918 
//    rlt N3ee510spin_mutexE             39.10398315 m  80000000 /  0: 10001318  1: 10001378  2:  9997679  3:  9996478  4: 10002363  5: 10001908  6:  9999910  7:  9998966 
//    rlt N3ee512spin_barrierE           39.27151398 m  80000000 /  0:  9998555  1: 10007830  2:  9999378  3:  9996263  4:  9998000  5:  9999385  6: 10001734  7:  9998855 
//    rlt N3ee517spin_shared_mutexI      39.29910535 m  80000000 /  0: 10002156  1:  9999819  2: 10001142  3:  9995567  4:  9999443  5: 10004356  6:  9999113  7:  9998404 
//
//

    


    
    
//-------------------------------------------------------------------------------------------------
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


class spin_posix
{
    pthread_spinlock_t l;

public:
    spin_posix()
    {
        pthread_spin_init(&l,0);
    }
    ~spin_posix()
    {
        pthread_spin_destroy(&l);
    }
    void lock()
    {
        pthread_spin_lock(&l);
    }
    void unlock()
    {
        pthread_spin_unlock(&l);
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


//-------------------------------------------------------------------------------------------------
// 32/64 bit traits for the shared reader / writer style lock.
//
// Notes:
//  
//  This idea is kinda based on the SRWLock from MSFT, but adapted to use the C++
//  support for threading. (aka a shared_mutex)
//
//  The idea of "max exclusive" writers is kindof "stupid" BUT if you have a system that
//  might have a large number of writers wanting exclusive access, you need to trap all
//  but the first one. Most of my designs typically have only a single writer, but in order
//  to keep this generally more useful the base class uses a traits system to allow for
//  the behavior to be modified. The default implementation allows for 65k (I told you, 
//  absurd) writers to hit the spin at the same time. My current hardware can only encounter 
//  a ~theoretical~ 8 way race and predictions are reasonable that we won't see 64bit 
//  CPU's with 65k cores in my life time. (It WOULD be cool to see this!)
//
//  One of the uses this class is good for is tracking "pending work" and blocking "new"
//  work. for those uses an "absurd" number of concurrent readers needs to be supported.
//  In this case the default on a 64bit system is a little over 1 billion "concurrent" readers.
//
//  Depending on usage, I have seen dozens of readers hold a lock for "a while." In (good)
//  concurrent design, you really should hold ANY lock for VERY short amounts of time.
//
struct is_64_bit
{
    static const bool value = sizeof(void*) == sizeof(uint64_t);

    using  value_type   = uint_fast64_t;
    using  barrier_type = std::atomic_uint_fast64_t;


    static constexpr value_type addend_exclusive    = 0x0000100000000000;
    static constexpr value_type mask_exclusive      = 0x1FFFF00000000000;
    static constexpr value_type max_exclusive       = 0x0FFFF00000000000;
    static constexpr value_type addend_shared       = 0x0000000000000001;
    static constexpr value_type mask_shared         = 0x000001FFFFFFFFFF;
    static constexpr value_type max_shared          = 0x000000FFFFFFFFFF;
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


//
// A writer waiting will stall NEW readers and any secondary++ writers waiting will stall until
// after all NEW readers blocked by the current writer finish.
//
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
    static constexpr storage_t max_exclusive    = storage_traits::max_exclusive;
    static constexpr storage_t addend_shared    = storage_traits::addend_shared;
    static constexpr storage_t mask_shared      = storage_traits::mask_shared;
    static constexpr storage_t max_shared       = storage_traits::max_shared;

    barrier_t  barrier;

public:
    spin_shared_mutex()
    {
        barrier = 0;
    }

    void lock()
    {
        // Spin until we acquire the write lock. Which we know is "ours" because
        // no one else owned it before.
        //
        while( ( barrier.fetch_add(addend_exclusive,acquire) & mask_exclusive ) > max_exclusive )
        {
            barrier.fetch_sub(addend_exclusive,relaxed);
        }

        // Spin while any readers are still owning a lock
        //
        while( ( barrier.load() & mask_shared ) > 0 );
    }

    void unlock()
    {
        barrier.fetch_sub(addend_exclusive,release);
    }

    bool try_lock()
    {
        // The only way this can succeed is if ALL writers and all readers
        // are not doing anything, which means that the value in the barrier
        // had to be zero.
        //
        bool owned = barrier.fetch_add(addend_exclusive,acquire) == 0;

        if( !owned )
        {
            barrier.fetch_sub(addend_exclusive,relaxed);
        }

        return owned;
    }

    void lock_shared()
    {
        // So "take" the lock,
        //
        storage_t value = ++barrier;

        // but spin until a "max read" slot is available OR while a writer
        // has the lock OR is waiting for readers to finish.
        //
        while( (value&mask_exclusive) || (value&mask_shared)>max_shared )
        {
            value = barrier.load(acquire);
        }
    }

    void unlock_shared()
    {
        barrier.fetch_sub(addend_shared,release);
    }

    bool try_lock_shared()
    {
        storage_t prev_value = barrier.fetch_add(addend_shared,acquire);

        bool owned = ((prev_value & mask_exclusive) == 0 ) && ((prev_value & mask_shared) <= max_shared );

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

#include <unistd.h>


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
