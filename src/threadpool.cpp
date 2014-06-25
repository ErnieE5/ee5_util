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
#include <system.h>
#include <error.h>
#include <logging.h>
#include <marshaling.h>
#include <stopwatch.h>
#include <threadpool.h>
#include <spin_locking.h>
#include <static_memory_pool.h>
#include <thread_support.h>
#include <workthread.h>


#include <cassert>
#include <algorithm>
#include <ctime>
#include <functional>
#include <list>
#include <vector>
#include <string>
#include <mutex>

BNS( ee5 )


static size_t ee5_thread_local the_thread_id;


size_t  work_thread_id()
{
    return the_thread_id;
}

void set_id(size_t i)
{
	the_thread_id = i;
}


template<typename B>
class TP : public B
{
private:
    //    cv_event chill;

    using mem_pool_t = static_memory_pool < 128, 1000000 >;

    struct deleter
    {
        // mem_pool_t* pool;
        TP* pool;

        deleter() : pool( nullptr )
        {
        }
        // deleter(mem_pool_t& p)      : pool(&p)      { }
        // deleter(const deleter& p)   : pool(p.pool)  { }
        deleter( TP& p ) : pool( &p )
        {
        }
        deleter( const deleter& p ) : pool( p.pool )
        {
        }

        void operator()( void* buffer )
        {
            reinterpret_cast<i_marshaled_call*>( buffer )->~i_marshaled_call();
            if( pool )
            {
                pool->mem.release( buffer );
                //                pool->chill.set();
                //                --pool->c;
            }
        }
    };

    using qitem_t = std::unique_ptr < i_marshaled_call, deleter >;
    using work_thread_t = WorkThread < qitem_t >;
    using tvec_t = std::vector < work_thread_t >;

    ee5_alignas( 64 ) spin_shared_mutex_t active;

    mem_pool_t          mem;
    tvec_t              threads;
    size_t              t_count;

    std::atomic_size_t  x;

protected:
    bool lock()
    {
        return active.try_lock_shared();
    }

    void unlock()
    {
        active.unlock_shared();
    }

    RC get_storage( size_t size, void** data )
    {
        CBREx( size <= mem_pool_t::max_item_size, e_to_large( mem_pool_t::max_item_size, "request is larger than pool size" ) );
        CBREx( data != nullptr, e_invalid_argument( 2, "value must be non-null" ) );

        *data = mem.acquire();

        if( !*data )
        {
            return e_pool_empty();
        }

        return s_ok();
    }

    RC enqueue_work( i_marshaled_call* p )
    {
        size_t v = std::rand() % t_count;
        //        size_t v = x%t_count;

        threads[v].Enqueue( qitem_t( p, deleter( *this ) ) );

        ++x;
        return s_ok();
    }


public:
    TP()
    {
        active.lock();
    }

    size_t Pending()
    {
        size_t s = 0;
        for( auto& k : threads )
        {
            s += k.Pending();
        }
        return s;
    }

    void Shutdown( bool abandon = false )
    {
        active.lock();

        for( auto& k : threads )
        {
            k.Quit();
        }
    }

    void Start( size_t t = std::thread::hardware_concurrency() )
    {
        std::srand( static_cast<unsigned int>( std::time( 0 ) ) );

        t_count = t;

        // Create the threads first...
        //
        for( size_t c = t_count; c; --c )
        {
            threads.push_back( work_thread_t( c, []( qitem_t& p ) { p->Execute(); } ) );
        }

        // Start them.
        for( auto& s : threads )
        {
            s.Startup();
        }

        active.unlock();
    }

    size_t Count()
    {
        return t_count;
    }

    void Park( size_t _c )
    {
        //        c=_c;
        //        chill.wait();
    }
};


//struct e
//{
//};
//using TP2 = marshal_work < TP<e> >;
//TP2 tp2;
static TP<i_marshal_work> tp;

async_call async;

i_marshal_work* async_call::tp;

void tp_start( size_t c )
{
    tp.Start( c );
    async.tp = &tp;
}
void tp_stop()
{
    tp.Shutdown();
}
size_t tp_pending()
{
    return tp.Pending();
}
size_t tp_count()
{
    return tp.Count();
}
void tp_park( size_t c )
{
    tp.Park( c );
}



ENS( ee5 )
