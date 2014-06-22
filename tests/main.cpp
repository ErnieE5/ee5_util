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


#include <stdio.h>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <iostream>

using namespace ee5;

#include "i_marshal_work.h"

template <class T, typename H = void>
struct pool_allocator
{
    typedef T               value_type;
    typedef T*              pointer;
    typedef const T*        const_pointer;
    typedef T&              reference;
    typedef const T&        const_reference;
    typedef std::size_t     size_type;
    typedef std::ptrdiff_t  difference_type;

    template<typename O> struct rebind
    {
        typedef pool_allocator<O> other;
    };

    pool_allocator(/*ctor args*/ )
    {
    };

    pool_allocator( const pool_allocator& )
    {
    };

    template< class O >
    pool_allocator( const pool_allocator<O, H>& other )
    {
        int y = sizeof( O );
        y = 1;
    };


    template< class A, class... Args >
    void construct( A* p, Args&&... args )
    {
        ::new( reinterpret_cast<void*>( p ) ) A( std::forward<Args>( args )... );
    }

    T* allocate( size_type n, const_pointer hint = 0 )
    {
        //        size_t s = sizeof(T);
        //        printf("%lu : %s\n",s,typeid(T).name());

        return reinterpret_cast<T*>( malloc( n * sizeof( T ) ) );
    }
    void deallocate( T* p, size_type n )
    {

    };
};


#include "static_memory_pool.h"
//#include <sys/mman.h>
//#include <unistd.h>
//#include <pthread.h>
//
//template< int IS, int IC, int A = cache_alignment_intel_x86_64 >
//class growing_memory_pool : private static_memory_pool<IS,IC,A>
//{
//private:
//    // This internal structure is mostly notational to avoid overuse of
//    // casting between the various usage of the memory.
//    //
//    struct pool_buffer
//    {
//        // When the buffer has been acquired by the user the entire byte range
//        // between the base and base+item_size is "yours to use" as if you called
//        // calloc(1,item_size);
//        //
//        // When the buffer (memory region) is "owned" by the cache, the member next is
//        // used for storage of items below in the stack,
//        //
//        union
//        {
//            unsigned char data[IS] __attribute__ ((aligned (A)));
//            pool_buffer*  next;
//        };
//    };
//
//
//public:
//    static const size_t max_item_size   = IS;
//    static const size_t max_item_count  = IC;
//    static const size_t cache_alignment = A;
//    static const size_t pool_size       = IC * sizeof(pool_buffer);
//
//    growing_memory_pool()
//    {
//        printf("_SC_LEVEL1_ICACHE_LINESIZE %lu\n",sysconf(_SC_LEVEL1_ICACHE_LINESIZE));
//        printf("_SC_LEVEL1_DCACHE_LINESIZE %lu\n",sysconf(_SC_LEVEL1_DCACHE_LINESIZE));
//        printf("_SC_LEVEL2_CACHE_LINESIZE  %lu\n",sysconf(_SC_LEVEL2_CACHE_LINESIZE));
//        printf("_SC_LEVEL3_CACHE_LINESIZE  %lu\n",sysconf(_SC_LEVEL3_CACHE_LINESIZE));
//        printf("_SC_LEVEL4_CACHE_LINESIZE  %lu\n",sysconf(_SC_LEVEL4_CACHE_LINESIZE));
//        printf("_SC_READER_WRITER_LOCKS    %lu\n",sysconf(_SC_READER_WRITER_LOCKS));
//        printf("_SC_CLOCK_SELECTION        %lu\n",sysconf(_SC_CLOCK_SELECTION));
//
////
//
//// pthread_condattr_t attr;
//// __clockid_t clock_id;
//// int ret;
//
//// ret = pthread_condattr_getclock(&attr &clock_id);
//
//        char data[1000];
//        confstr(_CS_GNU_LIBC_VERSION,data,sizeof(data));
//        printf("_CS_GNU_LIBC_VERSION %s\n",data);
//
//        confstr(_CS_GNU_LIBPTHREAD_VERSION,data,sizeof(data));
//        printf("_CS_GNU_LIBPTHREAD_VERSION %s\n",data);
//
//        confstr(_CS_PATH,data,sizeof(data));
//        printf("_CS_PATH %s\n",data);
//
//        confstr(_CS_POSIX_V7_LP64_OFF64_CFLAGS,data,sizeof(data));
//        printf("_CS_POSIX_V7_LP64_OFF64_CFLAGS %s\n",data);
//
//        confstr(_CS_V7_ENV,data,sizeof(data));
//        printf("_CS_V7_ENV %s\n",data);
//
//
//
//        size_t g = (sysconf(_SC_PAGESIZE)/sizeof(pool_buffer)   );
//
//        printf("%16lx  %lu\n",pool_size,g);
//
//
//
//        void* pmem = mmap(0,g*100,PROT_READ|PROT_WRITE, MAP_ANON|MAP_SHARED,-1,0);
//
//        printf("%16p\n",pmem);
//    }
//
//
//    void release(void* buffer)
//    {
//        static_memory_pool<IS,IC,A>::release(buffer);
//    }
//
//
//    void* acquire()
//    {
//        return static_memory_pool<IS,IC,A>::acquire();
//    }
//
//};
//
//
//void grow_test()
//{
//    growing_memory_pool<128,10000> d;
//
//}
//




//
//
//
//struct Foo
//{
//    void Bar(int x,int y)
//    {
//        printf("%i %i\n",x,y);
//    }
//
//    void Monkey(int x, double y)
//    {
//        printf("%i %lf\n",x,y);
//    }
//
//    int crap(int i)
//    {
//        printf("crap %i\n",i);
//        return i;
//    }
//
//};
//
//
//class m
//{
//    std::function<void()> call;
//
//public:
//    template<typename T,typename PM,typename...TArgs>
//    m(T* t,PM pm,TArgs...args) : call( std::bind(pm,t,args...) )
//    {
//    }
//
//    void operator()()
//    {
//        call();
//    }
//
//};
//
//
//
//template<typename T,typename...TArgs>
//struct Zed : public i_marshaled_call
//{
//    using PM = void(T::*)(TArgs...);
//    using PO = T*;
//
//    decltype( std::bind( std::declval<PM>(), std::declval<PO>(), std::declval<TArgs>() ... ) ) ff;
//
//    template<typename...CArgs>
//    Zed( T* pO, PM pM, CArgs&&...args ) : ff( std::bind(  pM, pO, args... ) )
//    {
//    }
//
//    void operator()()
//    {
//        ff();
//    }
//
//    void Execute()
//    {
//        ff();
//    }
//
//};
//
//
//
//void Func()
//{
//    printf("Hola!\n");
//}
//
//void T1()
//{
//    using namespace std::placeholders;
//    Foo bar;
//
//    using Gl = Zed<Foo,int,int>;
//
//    i_marshaled_call* p = new Gl( &bar, &Foo::Bar, 1, 2);
//
////    i_marshaled_call* p = new ( Func );
//
//
//
//
//    Gl z( &bar, &Foo::Bar, 1, 2);
//
//    printf("e1\n");
//    z();
//    printf("e2\n");
//    p->Execute();
//    printf("e3\n");
//
//    std::function<void()> f( std::bind( &Foo::Bar, &bar, 1, 2) );
//
//    std::tuple<int,double> x( 1,1.0 );
//
//
//    auto a1 = std::bind( &Foo::crap, &bar, _1);
//    object_method_delegate<Foo,int,int> a2(&bar,&Foo::crap);
//    auto a3 = std::bind( &Foo::crap, &bar, 7);
//
//    printf("a1: %lu == %i\n",sizeof(a1),a1(5) );
//    printf("a2: %lu == %i\n",sizeof(a2),a2(6) );
//    printf("a3: %lu == %i\n",sizeof(a3),a3()  );
//
//
////    bar.Monkey(x);
//
////    std::result_of< std::bind( (Foo::*)(int,int),Foo*,int,double,char> )>::type g;
//
//
//    //decltype( std::bind(&Foo::Bar, &bar, 1, 2) ) g;
//
//    auto f1 = std::bind( &Foo::Bar, _1, 1, 2);
//    m f2(&bar,&Foo::Bar, 1, 2);
//
//
//    auto f3 = std::bind( &Foo::crap, &bar, 22);
//
//    std::function<int(int)> f4 = std::bind( &Foo::crap, &bar, _1);
//
//    f();
//    f1(&bar);
//    f2();
//    printf("f3: %i\n",f3()   );
//    printf("f4: %i\n",f4(55) );
//
   // printf("dude\n");
//
//    exit(0);
//}


#include <workthread.h>
#include <array>











template<typename T>
class aysnc_writer
{
private:

protected:
public:
    aysnc_writer()
    {
    }

    RC write(void* b,size_t c)
    {
        return s_ok();
    }

};




//








struct foo
{
    foo* next;
    size_t thread;
    size_t value;
};


#include <alert_queue.h>



//std::condition_variable cv1;
//std::mutex park;

static_memory_pool<64, 2000> a;
//smp_queue_t<foo> q;
//smp_c_queue_t<foo> q;
smp_queue_t<foo> q;

cv_event zzzzz;

using work_thread_t = WorkThread< size_t >;


//class Test
//{
//private:
//    Test* ME()
//    {
//        return this;
//    }
//
//    void impl_SomeFunction()
//    {
//    }
//
//    //decltype( )
//
//protected:
//public:
//    static Test* glarf;
//
//
//    decltype( std::bind( &Test::impl_SomeFunction, std::declval<Test*>() ) ) async_SomeFunction;
//
//    Test() : async_SomeFunction( std::bind( &Test::impl_SomeFunction, this ) )
//    {
//    //    async_SomeFunction = ;
//
//        auto x = std::bind( &Test::impl_SomeFunction, this );
//
//        printf( "%s\n", typeid( decltype( std::bind( &Test::impl_SomeFunction, this ) ) ).name() );
//    }
//
//};

//-------------------------------------------------------------------------------------------------
//
//
//
//
int main()
{
    int iRet = ee5::Startup(0,nullptr);

    if( iRet == 0 )
    {
        LOG_ALWAYS("Good day!", "");

        //void tst_spin_locks();
        //tst_spin_locks();
        //void tst_atomic_queue();
        //tst_atomic_queue();
        void tst_threading();
        tst_threading();

        LOG_ALWAYS("Goodbye...", "");

        ee5::Shutdown();
    }

    return 0;

    int x = 0;

//    Test t;

    size_t readers = 1;
    size_t writers = 1;

    spin_flag f;

    auto tf = [&]()
    {
        for( size_t qq = 0; qq < 1000; ++qq )
        {
            f.lock();
            f.unlock();
            std::this_thread::sleep_for( std::chrono::nanoseconds( std::rand() % 1000 ) );
        }
    };


    s_stopwatch_f sw;
    std::thread t1( tf );
    std::thread t2( tf );

    t1.join();
    t2.join();

    printf( "Time: %.6f s\n", sw.delta() );

    return 0;


    std::vector<work_thread_t> dudes;

    auto remove = []( size_t x )
    {
        while( 1 )
        {
            foo* p = q.dequeue();

            if( p )
            {
                size_t n = p->thread;
                size_t v = p->value;
                printf( "%03lu:%03lu -- %7lu\n",x,n,v);
                delete p;
                if( v == 5555555 )
                {
                    return;
                }
            }
            else
            {
                printf( "Zzzzzz...\n" );
                fflush( stdout );
                zzzzz.set();
//                q.wait();
            }

        }

    };

    std::atomic_size_t nn;
    auto insert = [&]( size_t x )
    {
        for( size_t c = 0; c < 5000; ++c )
        {
            q.enqueue( new foo { nullptr, x, ++nn } );
            zzzzz.reset();
        }
    };

    for( size_t r = 0; r < readers; ++r )
    {
        dudes.push_back( work_thread_t( r, remove ) );
    }

    for( size_t i = 0; i < writers; ++i )
    {
        dudes.push_back( work_thread_t( i, insert ));
    }
    
    for( auto& t : dudes )
    {
        t.Startup();
    }

    for( size_t r = 0; r < readers; ++r )
    {
        dudes[r].Enqueue( std::move( r ) );
    }

    for( size_t i = readers; i < dudes.size(); ++i ) dudes[i].Enqueue( std::move( i ) );
    for( size_t i = readers; i < dudes.size(); ++i ) dudes[i].Enqueue( std::move( i ) );
    for( size_t i = readers; i < dudes.size(); ++i ) dudes[i].Enqueue( std::move( i ) );
    for( size_t i = readers; i < dudes.size(); ++i ) dudes[i].Enqueue( std::move( i ) );
    for( size_t i = readers; i < dudes.size(); ++i ) dudes[i].Enqueue( std::move( i ) );
    for( size_t i = readers; i < dudes.size(); ++i ) dudes[i].Enqueue( std::move( i ) );
    for( size_t i = readers; i < dudes.size(); ++i ) dudes[i].Enqueue( std::move( i ) );
    for( size_t i = readers; i < dudes.size(); ++i ) dudes[i].Enqueue( std::move( i ) );
    for( size_t i = readers; i < dudes.size(); ++i ) dudes[i].Enqueue( std::move( i ) );
    for( size_t i = readers; i < dudes.size(); ++i ) dudes[i].Enqueue( std::move( i ) );

    zzzzz.wait();

    for( size_t r = 0; r < readers; ++r )
    {
        q.enqueue( new foo { nullptr, r, 5555555 } );
    }

//    dudes[4].Enqueue( 555555 );

    for( auto& t : dudes )
    {
        t.Quit();
    }

    //foo b = { 1, 2 };

    //works = b;

    //foo a = works.load();





    return 0;
}



