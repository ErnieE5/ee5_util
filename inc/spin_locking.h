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

//-------------------------------------------------------------------------------------------------
// spinning locks
//
//  Using a spin lock in parallel code ~can~ be more efficient over a relatively long time. Much
//  of any gain is largely dependent on the load factor. If your program is well split up and the
//  scheduled operations are very short duration, the "win" ~can~ be as much as 4-5%. When I say
//  ~can~ it is just that. The win is over the VERY long run and is an average. Lots of things get
//  in the way if you are running on a preemptively scheduled OS. The gain over a mutex is largely
//  due to not incurring a call into the kernel to potentially park the thread. Adding "tons" of
//  threads for "complex computations" gets you no ware, slowly. For this algorithm and test,
//  "hyper threading" comes into play. For other tasks, this might not be as possible.
//
//  The benefit of a spin lock is considerably less when a "larger" amount of work per scheduled
//  work item is done. Anytime a work thread is preempted and the OS grabs the quantum during a
//  lock, the spike in time obliterates any benefit. (Keep your locks short!)
//
//  Another thing to keep in mind is locality of various locks. If you place two spin locks
//  on the same cache line, then those locks will have contention as well. i.e.:
//
//  Don't ~EVER~ put multiple "highly contentious" memory locking primitives together like this:
//
//      spin_mutex x;
//      spin_mutex y;
//      std::atomic<size_t> z;
//
//  The effect is that any will stall the other's access to the underlying control
//  variable. If the locks are exceptionally well used in the program (say a count of times
//  a particular branch is taken) the impact ~can~ be as much as 20% slower. Even if x and y
//  are "rarely" contentious, if ++z is frequently called, x & y will both stall briefly
//  during the increment. (This matters only for "exceptionally hot" memory locations.) The
//  CPU "stall" is due to the lock needing to make it all the way down to main memory. Different
//  memory speeds, cache architectures, bus speeds, cache levels, etc will contribute.
//
//  The std::mutex is the lowest common denominator and the C++11 primitives help make the best
//  of this tricky optimization.
//
//  In a nearly perfect asynchronous program, User code should ~never~ "park" a thread in a
//  thread pool.
//
// Note on VM's:
//
//  On virtual hardware (pretty much every cloud system in existence today), a spin lock can
//  end up being MORE costly than a typical mutex (or CriticalSection). Choose wisely and
//  always TEST with time on target before making a choice.
//
//  The statement that a spin lock is ALWAYS faster than a mutex is simply NOT TRUE when ALL
//  dependent factors are accounted for.
//
//  TODO: Move the following discussion ~and~ tests into a separate test file.
//
//  A few run tests with the "work" being a marginally expensive factorial calculation and the
//  difference in performance based on each work_items load. As expected, the gain is considerably
//  smaller the larger the work load. The actual work item ends up being mostly inline and the
//  code will largely stay in the local CPU cache. (Remember this test is mostly the locking
//  primitive and REAL code MUST account for larger amounts of memory movement between main memory
//  and the work_thread AND code movement in and out as well.)
//
//  Time: (8 Threads, Non-VM, "longer" running work items)
//
//      for(size_t q = 0;q < 200;q++)
//      {
//          ttf += ThreadpoolTest::Factorial(q*2);
//      }
//                                         Total Time     iterations  per thread thread scheduling with extra simple "random" assignment
//      rlt St5mutex                       39.47361948 m  80000000 /  0: 10000579  1: 10008047  2: 10003251  3:  9998440  4:  9994538  5:  9997243  6:  9996347  7: 10001555
//      rlt N3ee510spin_posixE             39.44335763 m  80000000 /  0:  9998613  1: 10000433  2:  9998422  3: 10008128  4:  9997107  5: 10000958  6:  9999421  7:  9996918
//      rlt N3ee510spin_mutexE             39.10398315 m  80000000 /  0: 10001318  1: 10001378  2:  9997679  3:  9996478  4: 10002363  5: 10001908  6:  9999910  7:  9998966
//      rlt N3ee512spin_barrierE           39.27151398 m  80000000 /  0:  9998555  1: 10007830  2:  9999378  3:  9996263  4:  9998000  5:  9999385  6: 10001734  7:  9998855
//      rlt N3ee517spin_shared_mutexI      39.29910535 m  80000000 /  0: 10002156  1:  9999819  2: 10001142  3:  9995567  4:  9999443  5: 10004356  6:  9999113  7:  9998404
//
//  Time: (8 Threads, Non-VM, "shorter" running work items)
//
//      for(size_t q = 0;q < 100;q++)
//      {
//          ttf += ThreadpoolTest::Factorial(q*2);
//      }
//                                     us spent waiting for lock                                                                                                          min,max us waiting
//                                     |                                                                                                                                                   |
//                                     |          total time     iterations   per thread thread scheduling with extra simple "random" assignment                                           |
//      rlt St5mutex                  -1513427977-103.26922607 m 800000000 /  0: 99998150  1:100006474  2:100000678  3:100016872  4: 99993981  5: 99992291  6: 99998268  7: 99993286 1 us (0,32822)
//      rlt N3ee510spin_posixE        - 161327492- 99.62528992 m 800000000 /  0: 99987630  1: 99998672  2:100005288  3:100008015  4: 99997749  5: 99996593  6: 99999365  7:100006688 0 us (0,48013)
//      rlt N3ee510spin_mutexE        - 165022104- 99.67398071 m 800000000 /  0: 99990276  1: 99997217  2: 99995870  3:100008688  4:100000098  5:100010531  6: 99988021  7:100009299 0 us (0,59955)
//      rlt N3ee512spin_barrierE      - 167674077- 99.74200439 m 800000000 /  0: 99994594  1:100026338  2: 99992586  3:100001379  4: 99990837  5:100000930  6: 99987491  7:100005845 0 us (0,43990)
//      rlt N3ee523spin_reader_writer - 247270275-100.27736664 m 800000000 /  0: 99996383  1: 99989494  2: 99989960  3:100016340  4:100002614  5:100013115  6: 99993705  7: 99998389 0 us (0,163977)
//



#ifndef EE5_SPIN_LOCKING_H_
#define EE5_SPIN_LOCKING_H_

#include <cassert>
#include <cstddef>
#include <atomic>
#include <thread> // TODO: currently needed for spin_posix

#include <stdio.h>

namespace ee5
{
//-------------------------------------------------------------------------------------------------
// spin_flag
//
//  The spin_flag is an exclusive lock that uses the atomic flag as the core locking method to
//  implement the barrier. Sometimes this can be faster because it is possible to save a reload
//  of the expected value. 
//
//  **Important** Note:
//  The data sizeof this element is a single byte on an intel platform. This is "cheap" for 
//  memory, but be EXTRA careful that the data that is packed NEAR the storage of this 
//  lock doesn't contain ANOTHER spin that can cuase REALLY weird deadlocks.
//
//  C++ concept: BasicLockable
//
class spin_flag
{
    std::atomic_flag barrier;

public:
    spin_flag() { barrier.clear(); }
    spin_flag(const spin_flag&) = delete;

    void lock()
    {
        while( barrier.test_and_set( std::memory_order_acquire ) );
    }

    void unlock()
    {
        barrier.clear( std::memory_order_release );
    }
};



//-------------------------------------------------------------------------------------------------
// spin_native
//
//  This routine uses the underlying POSIX routines to implement the spin lock. Can be a little
//  slower as the implementation isn't always inline optimized. This barrier will generally
//  be slightly faster (over time) than the std::mutex, but only slightly. This implementation is
//  mostly for completeness as other platforms could have different performance needs.
//
//  C++ concept: Lockable
//
#ifdef _MSC_VER
#define NOMINMAX
#include <windows.h>
class spin_native
{
    size_t l;


public:
    spin_native(const spin_native&) = delete;
    spin_native()
    {
        l = 0;
    }
    ~spin_native()
    {
    }
    void lock()
    {
        while (InterlockedCompareExchange(&l, 1, 0));
    }
    void unlock()
    {
        InterlockedExchange(&l, 0);
    }
    bool try_lock()
    {
        return InterlockedCompareExchange(&l, 1, 0) == 1;
    }
};
#else
class spin_native
{
    pthread_spinlock_t l;

public:
    spin_native(const spin_native&) = delete;
    spin_native()
    {
        pthread_spin_init(&l,0);
    }
    ~spin_native()
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
    bool try_lock()
    {
        return pthread_spin_trylock(&l) == 0;
    }
};
#endif


//-------------------------------------------------------------------------------------------------
// spin_mutex
//
//  A spin_mutex implements a mutex barrier (exclusive access). It is not reentrant. This is
//  intended for use where two or more threads on an SMP system need to modify / access a value and
//  the underlying modification is VERY short in nature. (Setting a few variables adding / removing
//  items from a container, etc.)
//
//  C++ concept: Lockable
//
class spin_mutex
{
    static const std::memory_order release = std::memory_order_release;
    static const std::memory_order relaxed = std::memory_order_relaxed;
    static const std::memory_order acquire = std::memory_order_acquire;

    std::atomic_bool barrier;

public:
    spin_mutex(const spin_mutex&) = delete;
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
//  spin_reader_writer_lock<>
//
// Notes:
//
//  This idea is kinda based on how the SRWLock from MSFT works, but adapted to use the C++
//  support for threading. (aka a shared_mutex)
//
//  The idea of "max exclusive" writers is kind-of "stupid" BUT if you have a system that
//  might have a large number of writers wanting exclusive access, you need to trap all
//  but the first one. Most of my designs typically have only a single writer, but in order
//  to keep this generally more useful the base class uses a traits system to allow for
//  the behavior to be modified. The default implementation allows for 65k (I told you,
//  absurd) writers to hit the spin at the same time. My current hardware can only encounter
//  a ~theoretical~ 8 way race and predictions are reasonable that we won't see 64bit
//  CPU's with 65k cores in my life time. (It WOULD however, be cool to see this!)
//
//  Basically, the maximum number of "spinners" is a function of the number of CPU's/cores
//  that have concurrent access to the memory bus. Each one of the memory access methods
//  atomically access the underlying memory during a single instruction. The OS can't
//  switch a thread context outside of a single instruction, therefore even if you have
//  thousands of writer threads, the total number can ONLY get that high IF (this is absurdly
//  unlikely, but not impossible) the OS thread quantum was set to the cycle / instruction level.
//  (A single chip with 64GB of "cache / main memory" and 8 cores might pull this off. I am
//  not going to hold my breath!)
//
//  One of the uses this class is good for is tracking "pending work" and blocking "new"
//  work. for those uses an "absurd" number of concurrent readers needs to be supported.
//  In this case the default on a 64bit system is a little over 1 billion "concurrent" readers.
//
//  Depending on usage, I have seen dozens of readers hold a lock for "a while." In (good)
//  concurrent design, you really shouldn't hold a lock very long. (For the case of the read
//  "locks" being a "trap" for pending work in a pool it's probably OK as the only time an
//  exclusive lock is taken is during a termination phase.)
//

//
// 32/64 bit traits for the shared reader / writer style lock.
//
struct is_32_bit
{
    static const bool value = sizeof(void*) == sizeof(uint32_t);

    using  value_type   = uint_fast32_t;
    using  barrier_type = std::atomic_uint_fast32_t;

    static const value_type addend_exclusive    = 0x00100000;
    static const value_type mask_exclusive      = 0x7FF00000;
    static const value_type max_exclusive       = 0x3FF00000;
    static const value_type mask_shared         = 0x0007FFFF;
    static const value_type max_shared          = 0x0003FFFF;
};

struct is_64_bit
{
    static const bool value = sizeof(void*) == sizeof(uint64_t);

    using  value_type   = uint_fast64_t;
    using  barrier_type = std::atomic_uint_fast64_t;

    static const value_type addend_exclusive    = 0x0000100000000000;
    static const value_type mask_exclusive      = 0x1FFFF00000000000;
    static const value_type max_exclusive       = 0x0FFFF00000000000;
    static const value_type mask_shared         = 0x000001FFFFFFFFFF;
    static const value_type max_shared          = 0x000000FFFFFFFFFF;
};



//-------------------------------------------------------------------------------------------------
// spin_reader_writer_lock
//
//  A reader / writer lock is used when a data structure is safe to access by many readers.
//  When a change needs to be made, a writer needs to take the lock exclusively so that any
//  changes are picked up by subsequent reads.
//
//  Exclusive access will stall NEW readers and block until existing readers are finished.
//
template<typename traits = typename std::conditional<is_64_bit::value,is_64_bit,is_32_bit>::type >
class spin_reader_writer_lock
{
private:
    static const std::memory_order release = std::memory_order_release;
    static const std::memory_order relaxed = std::memory_order_relaxed;
    static const std::memory_order acquire = std::memory_order_acquire;

    using storage_t = typename traits::value_type;
    using barrier_t = typename traits::barrier_type;

    static const storage_t addend_exclusive = traits::addend_exclusive;
    static const storage_t mask_exclusive   = traits::mask_exclusive;
    static const storage_t max_exclusive    = traits::max_exclusive;
    static const storage_t mask_shared      = traits::mask_shared;
    static const storage_t max_shared       = traits::max_shared;

    // Just a little safety for folks that might want to change the
    // behavior of the routines.
    //
    static_assert( mask_exclusive > mask_shared,                            "These routines require the exclusive bits in the higher order bytes.");
    static_assert( (mask_exclusive & addend_exclusive) == addend_exclusive, "Check your exclusive mask and addend!" );
    static_assert( (mask_exclusive > max_exclusive),                        "At least a single overflow bit is needed." );
    static_assert( (mask_shared & 0x1) == 0x1,                              "The shared mask must include the lowest order bit." );
    static_assert( (mask_shared > max_shared),                              "At least a single overflow bit is needed." );
    static_assert( (mask_shared & mask_exclusive) == 0,                     "No overlap of shared and exclusive bits allowed!" );

    barrier_t  barrier;

public:
    spin_reader_writer_lock(const spin_reader_writer_lock&) = delete;
    spin_reader_writer_lock()
    {
        barrier = 0;
    }

    void lock()
    {
        // Spin until we acquire the write lock. Which we know is "ours" because
        // no one else owned it before.
        //
        while( ( barrier.fetch_add(addend_exclusive,acquire) & mask_exclusive ) >= addend_exclusive )
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
        // Happy case is that no writers want in and on a 64bit system by default fewer than
        // 1 billion readers are using the lock.
        //
        while( ++barrier > max_shared )
        {
            // Unhappy we can't keep the lock
            //
            --barrier;

            // Stay away until there is a reasonable chance we can take the lock. This
            // prevents a lockout while an exclusive owner is waiting for readers to exit.
            //
            while(barrier > max_shared);
        }
    }

    void unlock_shared()
    {
        --barrier;
    }

    bool try_lock_shared()
    {
        bool owned = ++barrier <= max_shared;

        if(!owned)
        {
            --barrier;
        }

        return owned;
    }

};

using spin_shared_mutex_t = spin_reader_writer_lock<>;



}       // namespace ee5
#endif  // EE5_SPIN_LOCKING_H_




