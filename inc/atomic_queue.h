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

#ifndef ATOMIC_QUEUE_H_
#define ATOMIC_QUEUE_H_

#include <atomic>
#include <cassert>

//-------------------------------------------------------------------------------------------------
// atomic_queue
//
//  This is a simple template that manages a lock free queue based on more portable C++ 11 atomic
//  operations library. Use this template when the data structure contains a member value that
//  allows for storage of a pointer to the next item down.
//
//  For a larger number of programming strategies, using a data member to maintain linked lists
//  is not always the "best practice" for more "modern" styles of programming. This quite simple
//  template is a way to help add some type safety to store homogeneous sets of data. (In this
//  case it mostly means that the linked list pointers are in a uniform location / offset within
//  the structure.)
//
//  There are a gazillion more comments in this header than the code that is actually generated.
//  In optimized builds the vast majority of this falls away.
//
//  The only current requirement is that the type T has a public ~data~ member named 'next.'
//
//  TODO: (FUTURE:?)
//      Currently Clang is crashing if including self-referential declarations ~and~
//      the type T has an anonymous union ~and~ the -g option is used during the build!
//
//      The changes are simple. Uncomment the template declaration below, and add a couple of
//      asterisks in the item->*next calls.
//                             ^
//
//template<typename T,T* T::* next = &T::next>
template<typename T>
class atomic_queue
{
private:
    // These are re declarations because the verbosity of the
    // enumerations causes massively long lines.
    //
    static const std::memory_order release = std::memory_order_release;
    static const std::memory_order relaxed = std::memory_order_relaxed;

    std::atomic<T*> head; // Atomic storage for items ready for de-queue.
    std::atomic<T*> tail; // Atomic storage for items at the tail of the queue

    T* swap()
    {

        return nullptr;
    }


public:
    // 
    //
    void enqueue(T* item)
    {
        assert(item != nullptr);

        // There is a version of this routine that can save a local
        // on the stack. However, it was noted that some compilers
        // had issues with the ordering of the operations as of 2014
        // this is still likely a marginally more portable approach.
        //
        T* prior_tail = tail.load(relaxed);

        do
        {
            // Set item->next to value stored in top and
            // will hopefully ~still~ be the value in top
            // at the point of the exchange below.
            //
            item->next = prior_tail;
        } while (!tail.compare_exchange_weak(prior_tail, item, release, relaxed));
        //                                   |
        //                      if top STILL == prior_tail then item is placed in top
        //                      and true is returned otherwise
        //                      the new value of top replaces prior_top and false is
        //                      returned.
        //
    }

    // 
    //
    T* dequeu()
    {
        T* ret = head.load(relaxed);

        while ( ret != nullptr && !top.compare_exchange_weak(ret, ret->next, release, relaxed) );

        if ( !ret )
        {
            ret = swap();
        }

        return ret;
    }
};


#endif // ATOMIC_QUEUE_H_


