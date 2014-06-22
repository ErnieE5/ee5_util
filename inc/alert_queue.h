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
#pragma once
#include <ee5>

#include <cassert>
#include <condition_variable>
#include <spin_locking.h>
#include <workthread.h>

BNS( ee5 )
BNS( aq )
//-------------------------------------------------------------------------------------------------
// smp_queue
//
//  This is a simple template that manages a lock free queue based on more portable C++11 atomic
//  operations library. Use this template when the data structure contains a member value that
//  allows for storage of a pointer to the next item down.
//
//  For a larger number of programming strategies, using a data member to maintain linked lists
//  is not always the "best practice" for more "modern" styles of programming. This quite simple
//  template is a way to help add some type safety to store homogeneous sets of data. (In this
//  case it mostly means that the linked list pointers are in a uniform location / offset within
//  the structure.)
//
//  The only current requirement is that the type T has a public ~data~ member named 'next.'
//
template<typename T, typename B>
class ee5_alignas( CACHE_ALIGN ) smp_queue : public B
{
private:
    // These member variables are all very likely to fit within a cache line.
    // A spin lock is used because any thread that needs to modify the queue
    // will have to lock the data values anyway and the added complexity of
    // having separate locks would end up creating more stalls.
    //
    spin_flag   data_lock;
    T*          head;
    T*          tail;

public:
    template<typename...TArgs>
    smp_queue( TArgs...args ) : B( args... )
    {
        head = nullptr;
        tail = nullptr;
    }

    void enqueue( T* item )
    {
        item->next = nullptr;

        // In an optimized build this becomes a very simple set of
        // assembly instructions. Thank goodness for optimizing compilers!
        //
        framed_lock( data_lock, [&,this]()
        {
            // If we have a tail, just add the new item to the end
            //
            if( tail )
            {
                tail->next = item;
                tail = item;
            }
            else
            {
                assert( tail == nullptr && head == nullptr );

                // Otherwise, the container better be empty
                //
                head = tail = item;
            }
            B::inc();
        } );

        // If an alert is requested, then signal it
        //
        //  This is done outside of the lock, because we don't have any control over what the
        //  alert is. (It is recommended that it be something like an Event (Windows) or
        //  condition_variable (C++11).
        //
        B::set();
    }

    T* dequeue()
    {
        return framed_lock( data_lock, [&]()->T*
        {
            T* ret = head;

            if( ret )
            {
                head = head->next;
                if( !head )
                {
                    assert( tail == ret );
                    tail = nullptr;
                }
                B::dec();
            }
            else
            {
                B::reset();
            }

            return ret;
        } );
    }

    T* peek()
    {
        return head;
    }
};


class bare
{
protected:
    void set()
    {
    }
    void reset()
    {
    }
    void inc()
    {
    }
    void dec()
    {
    }
};


class counter : public bare
{
private:
    volatile size_t c;
protected:
    counter()
    {
        c = 0;
    }
    using bare::set;
    using bare::reset;
    void inc()
    {
        ++c;
    }
    void dec()
    {
        --c;
    }
public:
    size_t count()
    {
        return c;
    }
};


template<typename B>
class cv : public B
{
private:
    ee5::cv_event evnt;
protected:
    cv()
    {
    }
    void set()
    {
        evnt.set();
    }
    void reset()
    {
        evnt.reset();
    }
    using B::inc;
    using B::dec;
public:
    void wait()
    {
        evnt.wait();
    }
};



ENS( aq )

template<typename T>
using smp_queue_t = aq::smp_queue < T, aq::bare > ;

template<typename T>
using smp_c_queue_t = aq::smp_queue < T, aq::counter > ;

template<typename T>
using smp_cv_queue_t = aq::smp_queue < T, aq::cv< aq::bare > > ;

template<typename T>
using smp_ccv_queue_t = aq::smp_queue < T, aq::cv< aq::counter > > ;

ENS( ee5 )
