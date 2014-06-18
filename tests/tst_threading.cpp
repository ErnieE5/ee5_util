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

#include <error.h>
#include <logging.h>
#include <i_marshal_work.h>
#include <stopwatch.h>
#include <spin_locking.h>
#include <static_memory_pool.h>
#include <thread_support.h>
#include <workthread.h>

#include <cassert>

#include <algorithm>
#include <ctime>
#include <list>
#include <vector>
#include <string>
#include <mutex>

using namespace ee5;



//---------------------------------------------------------------------------------------------------------------------
//
//
//
//
class TP : public i_marshal_work
{
private:
    ThreadEvent chill;

    using mem_pool_t    = static_memory_pool<128,10000>;

    struct deleter
    {
        // mem_pool_t* pool;
        TP* pool;

        deleter()                   : pool(nullptr) { }
        // deleter(mem_pool_t& p)      : pool(&p)      { }
        // deleter(const deleter& p)   : pool(p.pool)  { }
        deleter(TP& p)              : pool(&p)      { }
        deleter(const deleter& p)   : pool(p.pool)  { }

        void operator()(void* buffer)
        {
            reinterpret_cast<i_marshaled_call*>(buffer)->~i_marshaled_call();
            if(pool)
            {
                pool->mem.release( buffer );
                if( pool->c )
                {
                    if(--(pool->c) == 0)
                    {
                        pool->chill.Set();
                    }
                }
            }
        }
    };

    using qitem_t       = std::unique_ptr<i_marshaled_call,deleter>;
    using work_thread_t = ee5::WorkThread<qitem_t>;
    using tvec_t        = std::vector<work_thread_t>;

    spin_shared_mutex_t active;

    mem_pool_t          mem;
    tvec_t              threads;
    size_t              t_count;
    std::atomic_size_t  x;

    std::atomic_size_t  c;

    bool lock()
    {
        return active.try_lock_shared();
    }

    void unlock()
    {
        active.unlock_shared();
    }

    RC get_storage(size_t size,void** pp)
    {
        if(size > mem_pool_t::max_item_size)
        {
            *pp = nullptr;
            return e_invalid_argument(1,"Pushing too much data.");
        }

        //         CBREx( size <= mem_pool::max_item_size, e_invalid_argument(2,"value must be non-null") );
        //         CBREx( pp != nullptr,                   e_invalid_argument(2,"value must be non-null") );
        *pp = nullptr;
        do
        {
            static std::atomic<size_t> c;
            static std::atomic<size_t> m;

            *pp = mem.acquire();

            if(!*pp)
            {
                return e_out_of_memory();
                m++;
            }
            else
            {
                c++;
            }
        }
        while(!*pp);

        return s_ok();
    }

    RC enqueue_work(i_marshaled_call* p)
    {
        threads[std::rand()%t_count].Enqueue( qitem_t( p, deleter(*this) ) );
        //threads[x%t_count].Enqueue( qitem_t( p, deleter(mem) ) );
        //threads[0].Enqueue( qitem_t( p, deleter(mem) ) );
        //threads[random()%8].Enqueue( qitem_t( p, deleter(mem) ) );
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
        for(auto& k : threads)
        {
            s += k.Pending();
        }
        return s;
    }

    void Shutdown(bool abandon = false)
    {
        active.lock();

        for(auto& k : threads)
        {
            k.Quit();
        }
    }

    void Start(size_t t = std::thread::hardware_concurrency())
    {
        std::srand(std::time(0));

        t_count = t;

        // Create the threads first...
        //
        for(size_t c = t_count; c ; --c)
        {
            threads.push_back( work_thread_t( c, [](qitem_t& p) { p->Execute(); } ) );
        }

        // Start them.
        for(auto& s : threads)
        {
            s.Startup();
        }

        active.unlock();
    }

    size_t Count()
    {
        return t_count;
    }

    void Park(size_t _c)
    {
        c=_c;
        chill.Chill();
    }
};


#include <functional>


TP tp;




//-------------------------------------------------------------------------------------------------
//
//
//
//
struct ThreadpoolTest
{
    ThreadpoolTest()
    {
    }
    ~ThreadpoolTest()
    {
    }

    void NineArgs(int a, int b, int c, int d, int e, int f, int g, int h, int i)
    {
        LOG_ALWAYS("a:%i b:%i c:%i d:%i e:%i f:%i g:%i h:%i i:%i",a,b,c,d,e,f,g,h,i);

        assert(a == 1);
        assert(b == 2);
        assert(c == 3);
        assert(d == 4);
        assert(e == 5);
        assert(f == 6);
        assert(g == 7);
        assert(h == 8);
        assert(i == 9);
    }

    void ScalarTypes(int a, double b,size_t c)
    {
        LOG_ALWAYS("a: %i b:%g c:%zu",a ,b ,c );
    }

    template<typename C>
    void TemplateRef(const C& items)
    {
        LOG_ALWAYS("(%zu)--", items.size() );
    }

    void ScalarAndReference(int a, double b, std::list<int>& l)
    {
        LOG_ALWAYS("(%zu) a:%i b:%g --", l.size(), a, b );
    }

    void String(std::string& s)
    {
        LOG_ALWAYS("s:%s", s.c_str() );
    }

    template<typename T>
    void Template(T t)
    {
        LOG_ALWAYS("value: TODO", "");
    }


    static long double Factorial(unsigned long n,long double a = 1)
    {
        if( n == 0 ) return a;
        return Factorial(n-1, a * n );
    }

    spin_mutex  lock;
    long double total = 0;

    void CalcFactorial(size_t n)
    {
        us_stopwatch_d taft;
        long double f = Factorial(n);

        for(unsigned long h = n;h > 2;h--)
        {
            tp.Async( [&](unsigned long h)
            {
                long double v = Factorial(h);
                framed_lock( lock, [&] { total+=v; } );
            }, h );
        }


        LOG_ALWAYS("n: %2lu value: %26.0Lf %8lu us",n, Factorial(n),taft.delta<std::micro>() );
    }

};




//---------------------------------------------------------------------------------------------------------------------
//
//
//
//
void c_function(size_t y)
{
    LOG_ALWAYS("y:%zu", y );
}



//---------------------------------------------------------------------------------------------------------------------
//
//
//
//
template<typename T>
void t_function(T t)
{
    LOG_ALWAYS("value: %s","TODO");
}




//---------------------------------------------------------------------------------------------------------------------
//
//
//
//
RC FunctionTests()
{
    // LOG_FRAME(0,"Thread pool %s","test!");
    // LOG_ALWAYS("%s","testing...");

    ThreadpoolTest  target;
    int             int_local       = 55555;
    double          double_local    = 55.555;

    TP& p = tp;

//using spin_shared_mutex_t = spin_reader_writer_lock<is_32_bit>;

    // grow_test();

    // return s_ok();

    // Scalar Types
    //
    CRR( p.Async( &target, &ThreadpoolTest::ScalarTypes, 1, 11.1, size_t(1000000000000000ull) ) );

    // Factorial Work Product
    //
    for(size_t n=0;n<26;++n)
    {
        CRR( p.Async( &target, &ThreadpoolTest::CalcFactorial, n ) );
    }

    // 9 Argument Support
    //
    CRR( p.Async( &target, &ThreadpoolTest::NineArgs, 1, 2, 3, 4, 5, 6, 7, 8, 9 ) );

    // RValue Forwarding to marshaling class
    //
    //CRR( p.Async( &target, &ThreadpoolTest::TemplateRef, std::list<double>( { 1.1, 1.2, 1.3, 1.4, 1.5, 1.6 } ) ) );
    //CRR( p.Async( &target, &ThreadpoolTest::TemplateRef, std::vector<int>(  { 1, 2, 3, 4, 5 } )                ) );
    //CRR( p.Async( &target, &ThreadpoolTest::String,      std::string("Ernie")                                  ) );


    // LValue Reference / Template
    //
    std::vector<double> dv( { 10.1, 10.2, 10.3, 10.4, 10.5, 10.6, 10.7, 10.8, 10.9 } );
    //CRR( p.Async( &target, &ThreadpoolTest::TemplateRef, dv ) );


    // Mixed Scaler Types and LValue Reference
    //
    std::list<int> li( { 1, 2, 3, 4, 5, 6 } );
    CRR( p.Async( &target, &ThreadpoolTest::ScalarAndReference, 1, 1.1, li ) );


    // Template Member Calls
    //
    CRR( p.Async( &target, &ThreadpoolTest::Template, std::string("Ewert")       ) );
    CRR( p.Async( &target, &ThreadpoolTest::Template, 5.5                        ) );
    CRR( p.Async( &target, &ThreadpoolTest::Template, 0x1000200030004000ll       ) );
    CRR( p.Async( &target, &ThreadpoolTest::Template, 0xF000E000D000C000         ) );
    CRR( p.Async( &target, &ThreadpoolTest::Template, static_cast<char>(0x45)    ) );
    CRR( p.Async( &target, &ThreadpoolTest::Template, &double_local              ) );

    // Scaler Types as references
    //
    //CRR( p.Async( &target, &ThreadpoolTest::ScalarTypes, int_local ,double_local, sizeof(unsigned)           ) );
    //CRR( p.Async( &target, &ThreadpoolTest::ScalarTypes, int_local ,double_local, sizeof(size_t)             ) );
    //CRR( p.Async( &target, &ThreadpoolTest::ScalarTypes, int_local ,double_local, sizeof(unsigned long)      ) );
    //CRR( p.Async( &target, &ThreadpoolTest::ScalarTypes, int_local ,double_local, sizeof(unsigned long long) ) );


    int h = 0;  // This value is just used for lambda capture testing in the following
                // tests that use lambda expressions.

    // Lambda Expression void(void) with capture by value of local
    // variable.
    //
    for( size_t c = 0; c < 5; ++c )
    {
        CRR( p.Async( [&]()
        {
            LOG_UNAME("Lambda [h]()","[h]:%i", h );
        } ) );
        ++h;
    }

    // Calling "naked" C function with a single argument
    //
    for(size_t i = 0;i < 2;++i)
    {
        CRR( p.Async( c_function, i ) );
    }

    // Template Function
    //
    CRR( p.Async( t_function<int>,       5   ) );
    CRR( p.Async( t_function<size_t>,    6   ) );



    std::atomic_ulong cc;   // This value is used to sum the number of calls to the lambda "q"
                            // and is done via a capture. The sum at the end of the test should
    cc = 0;                 // be equal to 7777777.

    // Lambda capture with three scalar arguments called locally
    // and queued later,
    //
    auto q = [&cc](int a,double b, int c)
    {
        long double ld = std::rand() * b;

        for(size_t i = 0;i < 1000;i++)
        {
            ld += ThreadpoolTest::Factorial(25);
        }

        // This should never happen, but the optimizer is too good
        // and will completely compile this code away if there is
        // NOT a chance that the calculated value is never used.
        //
        if( ld == 0 )
        {
            LOG_UNAME("Lambda q","ld: %20.20lg",ld);
        }

        //std::this_thread::yield();//sleep_for(std::chrono::nanoseconds(1));


        cc++; // The atomically incremented value
    };

    ++h;

    // Lambda capture (h) with three arguments, queued inline
    //
    CRR( p.Async( [h](int a,double b, int c)
    {
        LOG_UNAME("Lambda [h](int a,double b, int c)", "[h]:%i a:%i b:%g c:%i", h, a, b, c );
    }, 1 ,1.1, 1 ) );

    CRR( p.Async( q, 2, 2.2, 2 ) );


    LOG_ALWAYS("Starting...","");

    ms_stopwatch_f t1a;
    for(size_t g = 0;g < 2525252; g++)
    {
        CRR( p.Async( q, 3, g*.3, g ) );
    }
    LOG_ALWAYS("Phase One Complete... %5.3lf ms",t1a.delta<std::milli>());

    ms_stopwatch_f t2a;
    for(size_t g = 0;g < 5252524; g++)
    {
        CRR( p.Async( q, 4, g*.4, g ) );
    }
    LOG_ALWAYS("Phase Two Complete... %5.3lf ms",t2a.delta<std::milli>());

#ifdef _MSC_VER
#define alignas(x) __declspec(align(x))
#endif
    alignas(128) spin_barrier lock;

    std::list<size_t>   complete;
    auto join = [&lock,&complete](std::vector<int> add)
    {
        us_stopwatch_f x;
        framed_lock( lock, [&add,&complete]()
        {
            complete.push_back( add.size() );
            LOG_UNAME("join","size: %10lu add: %10lu",complete.size(),add.size());
        });

        LOG_UNAME("Elapsed Time mutex","%10.0lf us",x.delta());
    };

    // This test "adds" more time by creating a large amount of work
    // by doing "heap stuff" on a number of threads.
    //
    for(unsigned long long size = 1; size < 1000000000; size *= 10)
    {
        CRR( p.Async( [&]
        {
            us_stopwatch_f t;

            std::vector<int> v( size );

            int* addr = v.data();

            p.Async( [&](std::vector<int> v1)
            {
                us_stopwatch_f t;
                std::generate(v1.begin(),v1.end(), []{ return std::rand(); });
                LOG_UNAME("fart ","0x%.16lx Items %11lu, Time: %12.0f us inner", v1.data(),v1.size(),t.delta());

                p.Async( join, std::move(v1) );
            },
            std::move(v) );

            LOG_UNAME("fart ","0x%.16lx Items %11lu, Time: %12.0f us outer", addr, v.size(), t.delta() );

        } ) );
    }


    assert( cc == 7777777 );

    LOG_ALWAYS("cc == %lu", cc.load() );

    LOG_ALWAYS("Asta........","");

    return s_ok();
}



i_marshal_work* pool = &tp;

void    tp_start(size_t c)  { tp.Start(c);          }
void    tp_stop()           { tp.Shutdown();        }
size_t  tp_pending()        { return tp.Pending();  }
size_t  tp_count()          { return tp.Count();    }
void    tp_park(size_t c)   { tp.Park(c);           }


void tst_threading()
{
    h_stopwatch_d sw;

    tp.Start();

    FunctionTests();

    tp.Shutdown();

    // LOG_ALWAYS("%s","互いに同胞の精神をもって行動しなければならない。");
    // LOG_ALWAYS("%s","请以手足关系的精神相对待");

    LOG_ALWAYS("Elapsed time %.6f h",sw.delta());
}
