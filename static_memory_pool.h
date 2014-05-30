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

#include "atomic_stack.h"
#include <array>
#include <cstring>

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
//  cache_alignment:
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
template< int item_size, int item_count, int cache_alignment = cache_alignment_intel_x86_64 >
class static_memory_pool
{
    // This internal structure is mostly notational to avoid overuse of 
    // casting between the various usage of the memory.
    //
    struct pool_buffer
    {
        // When the buffer has been "handed out" to the "user" the entire byte range  
        // between the base and base+item_size is "yours to use" as if you called 
        // calloc(1,item_size); 
        //
        // When the buffer (memory region) is "owned" by the cache, the member next is 
        // used for storage of items below in the stack,
        //
        union
        {
            unsigned char data[item_size] __attribute__ ((aligned (cache_alignment)));
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
    storage_t   store = { };    // Use default initialization (effect is memset to zero)
    stack_t     cache;          // available storage buffers

    // General memory idea
    //
    // [Buffer][...]<Atomic pointer to "top" of available storage>
    //
    
public:

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

    // release a buffer back to the class and allow it to be
    // recycled.
    //
    bool release(void* buffer)
    {
        bool valid = is_valid_pointer(buffer);
        
        if( valid )
        {
            std::memset(buffer,0,item_size);
            
            cache.push( reinterpret_cast<pool_buffer*>(buffer) );
        }
        
        return valid;
    }
};


#endif // STATIC_MEMORY_POOL_H_