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

#include "system.h"
#include "logging.h"
#include "delegate.h"
#include "workthread.h"

#include "atomic_stack.h"
#include "static_memory_pool.h"

#include <stdio.h>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <type_traits>
#include <iostream>
#include <algorithm>
#include <list>
#include <string>
#include <vector>

#include <condition_variable>
#include <thread>
#include <mutex>

#include <atomic>


using namespace ee5;








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





#include <array>
#include <forward_list>







typedef static_memory_pool<128,10> mem_pool;

mem_pool mem;


void Moink()
{
    std::vector<int*> items;

    int k = 1;

    for( int* pi = mem.acquire<int>(); pi ; pi = mem.acquire<int>() )
    {
        *pi = k++;

        items.push_back(pi);
    }

    for(auto r : items)
    {
        mem.release(r);
    }

    assert( mem.release(&k) == false );

    struct pod
    {
        int x;
        unsigned y;
        double z;
    };

    {
        using pooled_pod = mem_pool::unique_type<pod>;
        std::forward_list< pooled_pod > managed;

        int      v = 0;
        double   d = .1;
        unsigned u = 0x10;

        auto f = mem.acquire_unique<pod>();

        for( pooled_pod pp = mem.acquire_unique<pod>() ; pp ; pp = mem.acquire_unique<pod>() )
        {
            pp->x = v++;
            pp->y = u++;
            pp->z = d;
            d += .1;
            managed.emplace_front( std::move(pp) );
        }

        for( auto& g : managed )
        {
            printf(" %2i %2u %2g\n",g->x,g->y,g->z);
        }
    }

}


//         typedef static_memory_pool<64,10>   mem_pool;
//         using pooled_int = mem_pool::unique_type<int>;
//
//
//
//         mem_pool pool;
//
//         auto a          = pool.acquire_unique<int>();
//         *a = 15;
//
//         pooled_int b    = pool.acquire_unique<int>();
//         *b = 30;



//         using foo = mem_pool::unique_type<unsigned>;
//         using get = foo (mem_pool::*)();
//         get acc = &mem_pool::acquire_unique<unsigned>;
//
//         if( std::is_member_function_pointer<get>::value )
//         {
    //             printf("dude!\n");
    //         }
    //
    //         auto ui = mem.acquire_unique<unsigned>();
    //
    //         mem_pool::unique_type<unsigned> q = mem.acquire_unique<unsigned>();
    //
    //         foo x = mem.acquire_unique<unsigned>();
    //         foo y = (mem.*acc)();
    //
    //
    //         *q  = 15;
    //         *ui = 8;

    //        printf("a: %u\nb: %u\n",*a,*b);



    //         std::shared_ptr<int> sh = mem.acquire_shared<int>();
    //
    //         *sh = 22;








#include "i_marshall_work.h"



template <class T,typename H = void>
struct pool_allocator
{
    typedef T               value_type;
    typedef T*              pointer;
    typedef const T*        const_pointer;
    typedef T&              reference;
    typedef const T&        const_reference;
    typedef std::size_t     size_type;
    typedef std::ptrdiff_t  difference_type;

    template<typename O> struct rebind { typedef pool_allocator<O> other; };

    pool_allocator(/*ctor args*/)
    {
    };

    pool_allocator(const pool_allocator&)
    {
    };

    template< class O >
    pool_allocator(const pool_allocator<O,H>& other)
    {
        int y =  sizeof(O);
        y = 1;
    };


    template< class A, class... Args >
    void construct( A* p, Args&&... args )
    {
        ::new( reinterpret_cast<void*>(p) ) A(std::forward<Args>(args)...);
    }

    T* allocate(size_type n,const_pointer hint = 0)
    {
        size_t s = sizeof(T);

//        printf("%lu : %s\n",s,typeid(T).name());

        return reinterpret_cast<T*>( malloc( n * sizeof(T) ) );
    }
    void deallocate(T* p, size_type n)
    {

    };
};




//---------------------------------------------------------------------------------------------------------------------
//
//
//
//
class TP : public i_marshal_work
{

private:
    using mem_pool_t    = static_memory_pool<128,100000>;
    using qitem_t       = std::shared_ptr<i_marshaled_call>;
    using work_thread_t = ee5::WorkThread<qitem_t>;
    using tvec_t        = std::vector<work_thread_t>;

    //using framed_lock_t = framed_lock<spin_mutex>;

    spin_mutex          termination_;

    std::atomic_flag    active;


    mem_pool_t          mem;
    tvec_t              threads;
    size_t              t_count = std::thread::hardware_concurrency() * 1;
    std::atomic_size_t  x;

    struct deleter
    {
        mem_pool_t* pool;

        deleter() = delete;
        deleter(mem_pool_t& p)      : pool(&p)      { }
        deleter(const deleter& p)   : pool(p.pool)  { }

        void operator()(void* buffer)
        {
            reinterpret_cast<i_marshaled_call*>(buffer)->~i_marshaled_call();
            pool->release( buffer );
        }
    };

    RC lock()
    {
//        barrier.lock();
        return s_ok();
    }

    void unlock()
    {
//        barrier.unlock();
    }

    RC get_storage(size_t size,void** pp)
    {
        if(size > mem_pool::max_item_size)
        {
            *pp = nullptr;
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
                //std::this_thread::sleep_for(std::chrono::milliseconds(1));
                std::this_thread::yield();
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
         threads[x%t_count].Enqueue( qitem_t( p, deleter(mem) ), nullptr);
         ++x;
         return s_ok();
    }

public:
    TP()
    {
//        printf("t_count: %lu\n",t_count);

        // Create the threads first...
        //
        for(size_t c = t_count; c ; --c)
        {
            threads.push_back( work_thread_t( c, [](qitem_t& p){ p->Execute(); } ) );
            printf("+%lu\n",c);
        }

        // Start them.
        for(auto& s : threads)
        {
            s.Startup();
        }
    }

    void Shutdown(bool abandon = false)
    {
        for(auto& k : threads)
        {
            k.Quit();
        }

        size_t s = 0;
        for(auto& k : threads)
        {
            s += k.Pending();
        }

        printf("-%lu\n",s);
    }

};


TP p;



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
    void TemplateRef(C& items)
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

    std::mutex  lock __attribute__ ((aligned (64)));

    std::array<int,64> g;

    long double total = 0;

    void CalcFactorial(unsigned long n)
    {
        Timer<> taft;
        long double f = Factorial(n);

        for(unsigned long h = n;h > 2;h--)
        {
            p.Async( [&](unsigned long h)
            {
                long double v = Factorial(h);
                framed_lock( lock, [&] { total+=v; } );
            }, h );
        }


        LOG_ALWAYS("n: %2lu value: %26.0Lf %5.0f us",n, Factorial(n),taft.Delta<std::micro>() );
    }
};



//---------------------------------------------------------------------------------------------------------------------
//
//
//
//
RC FunctionTests()
{
    LOG_FRAME(0,"Thread pool %s","test!");
    LOG_ALWAYS("%s","testing...");

    ThreadpoolTest  target;
    int             int_local       = 55555;
    double          double_local    = 55.555;

    Timer<> t;


    // Scalar Types
    //
    CRR( p.Async( &target, &ThreadpoolTest::ScalarTypes, 1, 11.1, 1000000000000000ul ) );


    // Factorial Work Product
    //
    for(size_t n=0;n<26;++n)
    {
        CRR( p.Async( &target, &ThreadpoolTest::CalcFactorial, n ) );
    }

    // Full 9 Argument Support
    //
    CRR( p.Async( &target, &ThreadpoolTest::NineArgs, 1, 2, 3, 4, 5, 6, 7, 8, 9 ) );


    // RValue Forwarding to marshaling class
    //
    CRR( p.Async( &target, &ThreadpoolTest::TemplateRef, std::list<float>( { 1.1, 1.2, 1.3, 1.4, 1.5, 1.6 }) ) );
    CRR( p.Async( &target, &ThreadpoolTest::TemplateRef, std::vector<int>( { 1, 2, 3, 4, 5 } )               ) );
    CRR( p.Async( &target, &ThreadpoolTest::String,      std::string("Ernie")                                ) );


    // LValue Reference / Template
    //
    std::vector<double> dv( { 10.1, 10.2, 10.3, 10.4, 10.5, 10.6, 10.7, 10.8, 10.9 } );
    CRR( p.Async( &target, &ThreadpoolTest::TemplateRef, dv ) );


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
    CRR( p.Async( &target, &ThreadpoolTest::ScalarTypes, int_local ,double_local, sizeof(unsigned)           ) );
    CRR( p.Async( &target, &ThreadpoolTest::ScalarTypes, int_local ,double_local, sizeof(size_t)             ) );
    CRR( p.Async( &target, &ThreadpoolTest::ScalarTypes, int_local ,double_local, sizeof(unsigned long)      ) );
    CRR( p.Async( &target, &ThreadpoolTest::ScalarTypes, int_local ,double_local, sizeof(unsigned long long) ) );


    int h = 0;  // This value is just used for lambda capture testing in the following
                // tests that use lambda expressions.

    // Lambda Expression void(void) with capture by value of local
    // variable.
    //
    for( size_t c = 0; c < 5; ++c )
    {
        CRR( p.Async( [h]()
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
        long double ld = random() * b;

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
    for(size_t g = 0;g < 2525252; g++)
    {
        CRR( p.Async( q, 3, g*.3, g ) );
    }
    LOG_ALWAYS("Phase One Complete...","");
    for(size_t g = 0;g < 5252524; g++)
    {
        CRR( p.Async( q, 4, g*.4, g ) );
    }
    LOG_ALWAYS("Phase Two Done.","");

    //std::array<int,64> foo;

    spin_mutex          lock;
    //spin_barrier        lock  __attribute__ ((aligned (64)));
    //std::mutex          lock;

    //std::array<int,64> bar;

    std::list<size_t>   complete;
    auto join = [&lock,&complete](std::vector<int> add)
    {
        Timer<> x;
        framed_lock( lock, [&add,&complete]()
        {
            complete.push_back( add.size() );
            LOG_UNAME("join","size: %10lu add: %10lu",complete.size(),add.size());
        });
        LOG_UNAME("Elapsed Time mutex","%10.0lf us",x.Delta());
    };

    // This test "adds" more time by creating a large amount of work
    // by doing "heap stuff" on a number of threads.
    //
    for(unsigned long long size = 1; size < 1000000000; size *= 10)
    {
        CRR( p.Async( [size,&join]
        {
            Timer<double> t;

            std::vector<int> v( size );

            int* addr = v.data();

            p.Async( [&](std::vector<int> v1)
            {
                Timer<double> t;
                std::generate(v1.begin(),v1.end(), []{ return random(); });
                LOG_UNAME("fart ","0x%.16lx Items %11lu, Time: %12.0lf us inner", v1.data(),
                            v1.size(),t.Delta());

                p.Async( join, std::move(v1) );
            },
            std::move(v) );

            LOG_UNAME("fart ","0x%.16lx Items %11lu, Time: %12.0lf us outer", addr, v.size(), t.Delta() );
        } ) );
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    printf("d00d!\n");

    p.Shutdown();

//    assert( cc == 7777777 );

    LOG_ALWAYS("cc == %lu", cc.load() );

    LOG_ALWAYS("Asta........","");

    return s_ok();
}


//#include<future>
//#include<numeric>
//
//typedef std::vector<int> vi;
//
//int parallel_sum(vi::iterator beg, vi::iterator end)
//{
//    typename vi::difference_type len = end-beg;
//    if(len < 1000)
//    {
//        return std::accumulate(beg, end, 0);
//    }
//
//    vi::iterator mid = beg + len/2;
//    auto handle = std::async( std::launch::async, parallel_sum, mid, end);
//    int sum = parallel_sum(beg, mid);
//    return sum + handle.get();
//}
//
//void foo()
//{
//    printf("Foo....\n");
//
//}



//---------------------------------------------------------------------------------------------------------------------
//
//
//
//
void App()
{
    LOG_FRAME(0,"Hello!","");
    {
        LOG_FRAME(0,"Embedded frame..","");
        LOG_ALWAYS("互いに同胞の精神を%s","もって行動しなければならない。");
    }

    Timer<>   taft;

    FunctionTests();
    LOG_ALWAYS("Elapsed Time: %lg seconds.",taft.Delta<std::ratio<1>>() );

    LOG_ALWAYS("%s","互いに同胞の精神をもって行動しなければならない。");
    LOG_ALWAYS("%s","请以手足关系的精神相对待");

    LOG_ALWAYS("%s","Almost done.");
}





//-------------------------------------------------------------------------------------------------
//
//
//
//
int main()
{
//   Moink();
//   int iRet = 0;
    int iRet = ee5::Startup(0,nullptr);

    if( iRet == 0 )
    {
        App();

        LOG_ALWAYS("Goodbye...","");

        ee5::Shutdown();
    }


    return iRet;
}

