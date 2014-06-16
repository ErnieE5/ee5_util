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

#ifndef STATIC_MEMORY_POOL_H_
#define STATIC_MEMORY_POOL_H_

#include <atomic_stack.h>

#include <memory>
#include <array>
#include <cstring>

namespace ee5
{

const int cache_alignment_intel_x86_64 = 64;

//-------------------------------------------------------------------------------------------------
// static_memory_pool
//
//  This is a static memory pool in the sense that the memory buffer establishes the storage
//  required for the object. It is quite plausible that you COULD place this object on the stack
//  or as a member of another object.
//
//  ALL memory handled out is zero initialized. The largest penalty is paid by the thread that
//  releases the buffer back into the pool. This is done for multiple reasons. The primary is
//  that it is generally safer (an opinion) to have memory conditioned to zero values. The
//  secondary reason is that "stale" data shouldn't hang around. (Security, etc.)
//
//  The "correct" use of this routine is up for debate, but the implementation is intended as
//  an SMP friendly base routine for well known sizes of transitory storage. Two intended uses
//  of this template are for thread-pool marshaling data, and network socket packet frame buffers.
//
//  All members of this class are thread safe EXCEPT the constructor. This class is SMP friendly
//  and designed to be called from any thread at any time after construction.
//
//  Notes:
//      Like with ANY memory management scheme, once the destruction of the class (i.e.
//      "freeing" the outer wrapper of the class) has started you should have already
//      made 100% certain that all of the "handed out" memory has been returned to the
//      class ~OR~ that it doesn't matter any more. Like with ANY memory management system,
//      if the users of this class impose other restrictions on the memory (say, like
//      the memory is used as an object and a destructor should be called) then it is the
//      responsibility of the higher level class to "deal with that."
//
//      *** This is NOT a general purpose heap and SHOULD NOT be used like one. It is intended
//      for either "short lived" buffers of well known size or as "reserved" storage for items
//      of uniform size that could cause heap fragmentation.
//
//
//  item_size:
//      The basic size of each of the "chunks" of memory handed out.
//
//  item_count:
//      The total number of items to reserve.
//
//  align:
//      The baseline alignment of each of the buffers "handed out" to the user of this
//      routine. For Intel systems running Linux in 2014 the cache line size is 64 bytes.
//      For the "best" usage of memory you SHOULD keep item_size as close to a multiple
//      of the cache_alignment as possible to avoid as much "waste" as possible. This is
//      a design choice based on a fundamental "user" of this class being thread pools
//      that likely want to avoid stalling on a cache line being held by another thread.
//      An additional benefit is that the proximity of following data is more likely to
//      already be mapped.
//
//
template< int item_size, int item_count, int align = cache_alignment_intel_x86_64 >
class static_memory_pool
{
private:
    // This internal structure is mostly notational to avoid overuse of
    // casting between the various usage of the memory.
    //
    struct pool_buffer
    {
        // When the buffer has been acquired by the user the entire byte range
        // between the base and base+item_size is "yours to use" as if you called
        // calloc(1,item_size);
        //
        // When the buffer (memory region) is "owned" by the cache, the member next is
        // used for storage of items below in the stack,
        //
        union
        {
            unsigned char data[item_size] __attribute__ ((aligned (align)));
            pool_buffer*  next;
        };
    };


    // The pool_buffer structure is mostly notational convenience, however this static assert
    // is used  to alert you if you are breaking the general assumption by introducing a member
    // before the data member.
    //
    static_assert( offsetof( pool_buffer, data ) == 0, "This storage is intended to have zero overhead.");

    typedef std::array<pool_buffer,item_count>  storage_t;
    typedef atomic_stack<pool_buffer>           stack_t;

    // The "simple" array of like sized buffers becomes a part of the memory layout of this
    // object.
    //
    storage_t       store = { };    // Use default initialization (effect is memset to zero)
    stack_t         cache;          // available storage buffers


    // This internal functor structure maintains a back reference to the instance
    // of the static_memory_pool that a returned unique_ptr uses to release a region
    // of memory handed out by the pool. Normally, binding the instance of the class
    // to a generic member function through a std::bind or ee5::object_method_delegate
    // would remove the need for this declaration. However, both of those methods (or
    // even a lambda) would require twice the storage. (The pointer to the object instance
    // and the pointer to the member / lambda.)
    //
    struct pool_deleter
    {
        static_memory_pool* pool;

        pool_deleter()                              : pool(nullptr) { }
        pool_deleter(static_memory_pool* p)         : pool(p)       { }
        pool_deleter(const static_memory_pool& p)   : pool(p.pool)  { }

        void operator()(void* buffer)
        {
            // The pool_deleter is private and the unique_ptr "mostly"
            // protects the user from doing "the wrong thing." This routine
            // bypasses the is_valid_pointer verification. Also, the unique_ptr
            // implementation already checks against nullptr. (Most optimizers
            // would be decent about removing this, but it is duplicated code.)
            //
            pool->internal_release( buffer );
        }
    };


    // release a buffer back to the class and allow it to be recycled.
    //
    void internal_release(void* buffer)
    {
        // Honor the zeroed memory contract
        //
        std::memset( buffer, 0, item_size );

        // Stash in cache.
        //
        cache.push( reinterpret_cast<pool_buffer*>( buffer ) );
    }


public:
    static const size_t max_item_size   = item_size;
    static const size_t max_item_count  = item_count;
    static const size_t cache_alignment = align;

    // Constructor
    //
    static_memory_pool()
    {
        // Load all of the items into the cache. The current effect of
        // this loop is that the end of the array is initially used first.
        //
        for(auto& i : store)
        {
            cache.push(&i);
        }
    }


    // A method to check if the pointer "belongs" to this class.
    //
    // The pointer only belongs IF it is in the proper range and the pointer
    // is properly aligned on the array boundary.
    //
    bool is_valid_pointer(void * buffer)
    {
        return
        ( reinterpret_cast<pool_buffer*>(buffer) >= &store.front() ) &&
        ( reinterpret_cast<pool_buffer*>(buffer) <= &store.back()  ) &&
        ((reinterpret_cast<ptrdiff_t>(buffer) - reinterpret_cast<ptrdiff_t>(store.data())) % sizeof(pool_buffer)) == 0;
    }


    // acquire a buffer
    //
    //  returns nullptr if no buffer is available.
    //
    void* acquire()
    {
        pool_buffer* ret = cache.pop();

        if( ret )
        {
            // Always assure that the memory is "clean"
            //
            ret->next = nullptr;
        }

        return ret;
    }


    // release a buffer back to the class and allow it to be
    // recycled.
    //
    bool release(void* buffer)
    {
        bool valid = is_valid_pointer(buffer);

        if( valid )
        {
            internal_release(buffer);
        }

        return valid;
    }


    // acquire a buffer as a specific type.
    //
    //  returns nullptr if no buffer is available.
    //
    //  This routine is mostly a notational convenience.
    //
    //          int* value = pool.acquire<int>();
    //      vs.
    //          int* value = reinterpret_cast<int*>( pool.acquire() );
    //
    template<typename T>
    T* acquire()
    {
        return reinterpret_cast<T*>( acquire() );
    }


    //---------------------------------------------------------------------------------------------
    // unique_ptr support
    //
    //      unique_ptr support is only enabled if the type is a POD type.
    //      (Plain Old Data.) This class is intended for "higher performance"
    //      applications where other types of "magic" may be happening in
    //      wrappers that use this implementation as a foundation. Directly
    //      adding support for things like shared_ptr and other more advanced
    //      features are beyond the scope of what ~this~ implementation is
    //      intended to be used for.
    //
    //      A typedef or using alias can simplify the usage from a
    //      verbosity perspective.
    //
    //  Examples:
    //      typedef static_memory_pool<64,10>   mem_pool;
    //      using pooled_int = mem_pool::unique_type<int>;
    //
    //      mem_pool pool;
    //
    //      auto a          = pool.acquire_unique<int>();
    //      *a = 15;
    //
    //      pooled_int b    = pool.acquire_unique<int>();
    //      *b = 30;
    //
    //      struct simple
    //      {
    //          int         x;
    //          unsigned    y;
    //          double      z;
    //      }
    //
    //     using pooled_simple = mem_pool::unique_type<simple>;
    //     std::forward_list< pooled_simple > managed;
    //
    //     int      q = 0;
    //     unsigned e = 0x10;
    //     double   d = .1;
    //
    //     for( pooled_pod pp = mem.acquire_unique<pod>() ; pp ; pp = mem.acquire_unique<pod>() )
    //     {
    //         pp->x = q++;
    //         pp->y = e++;
    //         pp->z = d;
    //         d += .1;
    //         managed.emplace_front( std::move(pp) );
    //     }
    //
    //---------------------------------------------------------------------------------------------


    // Can help simplify the declaration of a unique_ptr handler for the pool.
    //
    template<typename T>
    using unique_type = std::unique_ptr<T,pool_deleter>;


    // acquire a unique_ptr to manage the lifetime of the buffer.
    //
    //  An explicit release is NOT required.
    //
    template<typename T, typename...TArgs>
    unique_type<T> acquire_unique(TArgs...args)
    {
        return unique_type<T>( new(acquire()) T(std::forward<TArgs>(args)...), pool_deleter(this) );
    }
};


}       // namespace ee5
#endif  // STATIC_MEMORY_POOL_H_