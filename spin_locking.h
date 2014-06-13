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
//  While doing parallel coding using a spin lock ~can~ be more efficient over a relatively
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
// Note on VM's:
//
//  On virtual hardware (pretty much every cloud system in existence today), a spin lock can
//  end up being MORE costly than a typical mutex (or CriticalSection). Choose wisely and
//  always TEST with time on target before making a choice.
//
//  The statement that a spin lock is ALWAYS faster than a mutex is simply NOT TRUE when ALL
//  dependent factors are accounted for.
//
//
//
//
//  Time: (8 Threads, Non-VM, "longer" running work items)
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




#ifndef EE5_SPIN_LOCKING_H_
#define EE5_SPIN_LOCKING_H_

#include <cassert>
#include <cstddef>
#include <atomic>
#include <thread> // TODO: currently needed for spin_posix

namespace ee5
{
//-------------------------------------------------------------------------------------------------
// spin_barrier
//
//  The spin_barrier is an exclusive lock that uses the atomic flag as the core locking method to
//  implement the barrier. Sometimes this can be faster because it is possible to save a reload
//  of the expected value.
//
//  C++ concept: BasicLockable
//
class spin_barrier
{
    std::atomic_flag barrier = ATOMIC_FLAG_INIT;

public:
    spin_barrier() { }
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


// TODO: add guard when POSIX is missing

//-------------------------------------------------------------------------------------------------
// spin_posix
//
//  This routine uses the underlying POSIX routines to implement the spin lock. Can be a little
//  slower as the implementation isn't always inline optimized. This barrier will generally
//  be slightly faster (over time) than the std::mutex, but only slightly. This implementation is
//  mostly for completeness as other platforms could have different performance needs.
//
//  C++ concept: BasicLockable
//
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
//  thousands of writer threads, the total number can ONLY get that high IF (this is REALLY
//  unlikely, but not impossible!) the OS quantum was at the instruction level.
//
//  One of the uses this class is good for is tracking "pending work" and blocking "new"
//  work. for those uses an "absurd" number of concurrent readers needs to be supported.
//  In this case the default on a 64bit system is a little over 1 billion "concurrent" readers.
//
//  Depending on usage, I have seen dozens of readers hold a lock for "a while." In (good)
//  concurrent design, you really shouldn't hold a lock very long.
//

//
// 32/64 bit traits for the shared reader / writer style lock.
//
struct is_32_bit
{
    static const bool value = sizeof(void*) == sizeof(uint32_t);

    using  value_type   = uint_fast32_t;
    using  barrier_type = std::atomic_uint_fast32_t;

    static constexpr value_type addend_exclusive    = 0x00100000;
    static constexpr value_type mask_exclusive      = 0x7FF00000;
    static constexpr value_type max_exclusive       = 0x3FF00000;
    static constexpr value_type addend_shared       = 0x00000001;
    static constexpr value_type mask_shared         = 0x0007FFFF;
    static constexpr value_type max_shared          = 0x0003FFFF;
};

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



//
// A writer waiting will stall NEW readers and any secondary++ writers waiting will stall until
// after all NEW readers blocked by the current writer finish.
//
template<typename storage_traits = typename std::conditional<is_64_bit::value,is_64_bit,is_32_bit>::type >
class spin_reader_writer_lock
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
    spin_reader_writer_lock()
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

using spin_shared_mutex_t = spin_reader_writer_lock<>;



}       // namespace ee5
#endif  // EE5_SPIN_LOCKING_H_




