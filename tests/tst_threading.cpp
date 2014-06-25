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

using namespace ee5;

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

    void NineArgs( int a, int b, int c, int d, int e, int f, int g, int h, int i )
    {
        LOG_ALWAYS( "a:%i b:%i c:%i d:%i e:%i f:%i g:%i h:%i i:%i", a, b, c, d, e, f, g, h, i );

        assert( a == 1 );
        assert( b == 2 );
        assert( c == 3 );
        assert( d == 4 );
        assert( e == 5 );
        assert( f == 6 );
        assert( g == 7 );
        assert( h == 8 );
        assert( i == 9 );
    }

    void ScalarTypes( int a, double b, size_t c )
    {
        LOG_ALWAYS( "a: %i b:%lf c:%llu", a, b, c );
    }

    template<typename C>
    void TemplateRef( C& items )
    {
        LOG_ALWAYS( "(%llu)--", items.size() );
    }

    void ScalarAndContainer( int a, double b, std::list<int> l )
    {
        LOG_ALWAYS( "(%llu) a:%i b:%g --", l.size(), a, b );
    }

    void CopyString( std::string& s )
    {
        LOG_ALWAYS( "s:%s", s.c_str() );
    }

    void MoveString( std::string s )
    {
        LOG_ALWAYS( "s:%s", s.c_str() );
    }

    template<typename T>
    void Template( T t )
    {
        LOG_ALWAYS( "value: TODO", "" );
    }


    static long double Factorial( size_t n, long double a = 1 )
    {
        if( n == 0 ) return a;
        return Factorial( n - 1, a * n );
    }

    spin_mutex  lock;
    long double total = 0;

    void CalcFactorial( size_t n )
    {
        LOG_ALWAYS( "n: %2lu value: %26.0Lf", n, Factorial( n ) );
    }

};




//---------------------------------------------------------------------------------------------------------------------
//
//
//
//
void c_function( size_t y )
{
    LOG_ALWAYS( "y:%llu", y );
}



//---------------------------------------------------------------------------------------------------------------------
//
//
//
//
template<typename T>
void t_function( T t )
{
    LOG_ALWAYS( "value: %s", "TODO" );
}


//---------------------------------------------------------------------------------------------------------------------
//
//
//
//
std::array<size_t, 9> p10 = { { 1ull, 10ull, 100ull, 1000ull, 10000ull, 100000ull, 1000000ull, 10000000ull, 100000000ull } };
struct rvalue_test
{
    using lsize_t = std::vector < size_t > ;

    ee5_alignas(64)
    std::atomic_size_t          pending;
    std::array<ptrdiff_t, 9>    rvalue_transfers;
    ee5_alignas( 64 )
    spin_flag                   lock;
    lsize_t                     complete;


    void join( lsize_t add )
    {
        us_stopwatch_f x;
        framed_lock( lock, [&]()
        {
            LOG_UNAME( "jtst", "0x%.16lx Items %11lu  Time: %12.0f us join delay", add.data(), add.size(), x.delta() );
            complete.push_back( add.size() );
        } );
        LOG_UNAME( "jtst", "0x%.16lx Items %11lu  Time: %12.0f us join", add.data(), add.size(), x.delta() );

        ptrdiff_t a = reinterpret_cast<ptrdiff_t>( add.data() );
        ptrdiff_t b = rvalue_transfers[add[0]];

        LOG_UNAME( "jtst", "0x%.16lx Items %11lu %s 0x%.16lx", a, add.size(), a == b ? "==" : "!=", b );
        --pending;
    }

    void inner( lsize_t v1 )
    {
        us_stopwatch_f t;
        std::generate( v1.begin() + 1, v1.end(), []()->size_t{ return std::rand(); } );
        LOG_UNAME( "jtst", "0x%.16lx Items %11lu  Time: %12.0f us inner", v1.data(), v1.size(), t.delta() );

        p->Async( &rvalue_test::join, this, std::move( v1 ) );
    }

    void outer( size_t num )
    {
        us_stopwatch_f t;

        lsize_t v( p10[num] );

        v[0] = num;

        rvalue_transfers[num] = reinterpret_cast<ptrdiff_t>( v.data() );

        size_t* addr = v.data();

        p->Async( &rvalue_test::inner, this, std::move( v ) );

        LOG_UNAME( "jtst", "0x%.16lx Items %11lu  Time: %12.0f us outer", addr, p10[num], t.delta() );
    };

    i_marshal_work* p;

    rvalue_test( i_marshal_work* _p ) : p( _p )
    {
        pending = 0;

        // This test "adds" more time by creating a large amount of work
        // by doing "heap stuff" on a number of threads.
        //
        for(size_t size = 0; size < p10.size(); ++size)
        {
            p->Async( &rvalue_test::outer, this, size );
            ++pending;
        }

        while( pending )
        {
            std::this_thread::yield();
        }
    }

};





//---------------------------------------------------------------------------------------------------------------------
//
//
//
//
void tst_rvalue_transfers( i_marshal_work* p )
{
    ee5_alignas( 64 )
    std::atomic_size_t pending;
    std::array<ptrdiff_t, 9> rvalue_transfers;
    ee5_alignas( 64 )
    spin_flag lock;
    using lsize_t = std::vector < size_t > ;
    lsize_t   complete;

    pending = 0;

    auto join = [&]( lsize_t add )
    {
        us_stopwatch_f x;
        framed_lock( lock, [&]()
        {
            LOG_UNAME( "jtst", "0x%.16lx Items %11lu  Time: %12.0f us join delay", add.data(), add.size(), x.delta() );
            complete.push_back( add.size() );
        } );
        LOG_UNAME( "jtst", "0x%.16lx Items %11lu  Time: %12.0f us join", add.data(), add.size(), x.delta() );

        ptrdiff_t a = reinterpret_cast<ptrdiff_t>( add.data() );
        ptrdiff_t b = rvalue_transfers[add[0]];

        LOG_UNAME( "jtst", "0x%.16lx Items %11lu %s 0x%.16lx", a, add.size(), a == b ? "==" : "!=", b );
        --pending;
    };

    auto inner = [&]( lsize_t v1 )
    {
        us_stopwatch_f t;
        std::generate( v1.begin() + 1, v1.end(), []()->size_t{ return std::rand(); } );
        LOG_UNAME( "jtst", "0x%.16lx Items %11lu  Time: %12.0f us inner", v1.data(), v1.size(), t.delta() );

        p->Async( join, std::move( v1 ) );
    };

    auto outer = [&]( size_t num )
    {
        us_stopwatch_f t;

        lsize_t v( p10[num] );

        v[0] = num;

        rvalue_transfers[num] = reinterpret_cast<ptrdiff_t>( v.data() );

        size_t* addr = v.data();

        p->Async( inner, std::move( v ) );

        LOG_UNAME( "jtst", "0x%.16lx Items %11lu  Time: %12.0f us outer", addr, p10[num], t.delta() );
    };

    // This test "adds" more time by creating a large amount of work
    // by doing "heap stuff" on a number of threads.
    //
    for(size_t size = 0; size < p10.size(); ++size)
    {
        p->Async( outer, size );
        ++pending;
    }

    while( pending )
    {
        std::this_thread::yield();
    }
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
    int             int_local = 55555;
    double          double_local = 55.555;

    //us_stopwatch_s tpmf;
    //rvalue_test x( &tp );
    //size_t pmftime = tpmf.delta();

    //us_stopwatch_s tlambda;
    //tst_rvalue_transfers( &tp );
    //size_t lambdas = tlambda.delta();

    //LOG_UNAME( "jtst", "Labmda's   :%llu us", lambdas );
    //LOG_UNAME( "jtst", "PMF's      :%llu us", pmftime );

    //while( tp.Pending() )
    //{
    //    std::this_thread::yield();
    //}

    async( [](){ LOG_ALWAYS( "", "" ); } );

    async( [&]() { LOG_ALWAYS( "Local value (captured) %i %lf", int_local, double_local ); } );

    // This is "dumb" but it works.
    //
    async( [&](void(ThreadpoolTest::* pmf)(int,double,size_t ))
    {
        (target.*pmf)( 5, 5.5, 5555555555ull );

    }, &ThreadpoolTest::ScalarTypes );


    // Scalar Types
    //
    CRR( async( &ThreadpoolTest::ScalarTypes, &target, 1, 11.1, size_t( 1000000000000000ull ) ) );


    // Factorial Work Product
    //
    for(size_t n=1;n<26;++n)
    {
        CRR( async( &ThreadpoolTest::CalcFactorial, &target, n ) );
    }

    // 9 Argument Support
    //
    CRR( async( &ThreadpoolTest::NineArgs, &target, 1, 2, 3, 4, 5, 6, 7, 8, 9 ) );


    std::string s1( "Foo" );
    std::vector<double> dv( { 10.1, 10.2, 10.3, 10.4, 10.5, 10.6, 10.7, 10.8, 10.9 } );

     CRR( async->AsyncByVal( &ThreadpoolTest::TemplateRef,&target,  std::list<double>( { 1.1, 1.2, 1.3, 1.4, 1.5, 1.6 } ) ) );
     //CRR( asyncByVal( &ThreadpoolTest::TemplateRef,&target,  std::vector<int>(  { 1, 2, 3, 4, 5 } )                ) );
     //CRR( asyncByVal( &ThreadpoolTest::TemplateRef,&target,  dv                                                    ) );
     //CRR( asyncByVal( &ThreadpoolTest::CopyString,&target,  s1 ) );


    CRR( async( &ThreadpoolTest::MoveString, &target, std::string( "Ernie" ) ) );

    //
    //
    std::list<int> li( { 1, 2, 3, 4, 5, 6 } );
    CRR( async( &ThreadpoolTest::ScalarAndContainer, &target, 1, 1.1, std::move( li ) ) );


    // Template Member Calls
    //
    CRR( async( &ThreadpoolTest::Template<std::string>, &target, std::string( "Ewert" ) ) );
    CRR( async( &ThreadpoolTest::Template, &target, 5.5 ) );
    CRR( async( &ThreadpoolTest::Template, &target, 0x1000200030004000ll ) );
    CRR( async( &ThreadpoolTest::Template, &target, 0xF000E000D000C000 ) );
    CRR( async( &ThreadpoolTest::Template, &target, static_cast<char>( 0x45 ) ) );
    CRR( async( &ThreadpoolTest::Template, &target, &double_local ) );

    // Scaler Types as references
    //
    CRR( async( &ThreadpoolTest::ScalarTypes, &target, int_local, double_local, sizeof( unsigned ) ) );
    CRR( async( &ThreadpoolTest::ScalarTypes, &target, int_local, double_local, sizeof( size_t ) ) );
    CRR( async( &ThreadpoolTest::ScalarTypes, &target, int_local, double_local, sizeof( unsigned long ) ) );
    CRR( async( &ThreadpoolTest::ScalarTypes, &target, int_local, double_local, sizeof( unsigned long long ) ) );


    int h = 0;  // This value is just used for lambda capture testing in the following
    // tests that use lambda expressions.

    // Lambda Expression void(void) with capture by value of local
    // variable.
    //
    for( size_t c = 0; c < 5; ++c )
    {
        CRR( async( [&]()
        {
            LOG_UNAME("Lambda [h]()","[h]:%i", h );
        } ) );
        ++h;
    }

    // Calling "naked" C function with a single argument
    //
    for(size_t i = 0;i < 2;++i)
    {
        CRR( async( c_function, i ) );
    }

    //// Template Function
    ////
    CRR( async( t_function<int>, 5 ) );
    CRR( async( t_function<size_t>, 6 ) );


    ee5_alignas( 128 )
    std::atomic_ulong cc;   // This value is used to sum the number of calls to the lambda "q"
                            // and is done via a capture. The sum at the end of the test should
    cc = 0;                 // be equal to 7777777.

    // Lambda capture with three scalar arguments called locally
    // and queued later,
    //
    auto q = [&cc](size_t a,double b)
    {
        long double ld = std::rand() * b;

        for(size_t i = 0;i < 10000;i++)
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

        cc++; // The atomically incremented value
    };

    ++h;

    // Lambda capture (h) with three arguments, queued inline
    //
    CRR( async( [h]( int a, double b, int c )
    {
        LOG_UNAME("Lambda [h](int a,double b, int c)", "[h]:%i a:%i b:%g c:%i", h, a, b, c );
    }, 1 ,1.1, 1 ) );

    CRR( async( q, 2, 2.2 ) );


    LOG_ALWAYS("Starting...","");

    ms_stopwatch_f t1a;
    for(size_t g = 0;g < 2525252; g++)
    {
        RC rc;
        do
        {
            rc = async( q, 3, g*.3 );
            if(rc!=s_ok())
            {
                std::this_thread::yield();
            }
        }
        while( s_ok() != rc );
    }
    LOG_ALWAYS("Phase One Complete... %5.3lf ms",t1a.delta<std::milli>());

    ms_stopwatch_f t2a;
    for(size_t g = 0;g < 5252524; g++)
    {
        RC rc;
        do
        {
            rc = async( q, 4, g*.4 );
            if(rc!=s_ok())
            {
                std::this_thread::yield();
            }
        }
        while( s_ok() != rc );
    }
    LOG_ALWAYS( "Phase Two Complete... %5.3lf ms", t2a.delta<std::milli>() );


    while( tp_pending() )
    {
        std::this_thread::yield();
    }

    //     assert( cc == 7777777 );
    //
    //     LOG_ALWAYS("cc == %llu", cc.load() );

    LOG_ALWAYS( "Asta........", "" );


    return s_ok();
}








//---------------------------------------------------------------------------------------------------------------------
//
//
//
//
void tst_threading()
{
    s_stopwatch_d sw;

    //     printf("%lu\n",sizeof(ptrdiff_t));
    //     printf("%lu\n",sizeof(long));
    //
    //     using lsize_t = std::vector<ptrdiff_t>;
    //     lsize_t v1(1000);
    //     printf("v1 %p\n",v1.data());
    //
    //     lsize_t v2( std::move(v1) );
    //     printf("v2 %p\n",v2.data());
    //
    //     lsize_t v3( std::move(v2) );
    //     printf("v3 %p\n",v3.data());
    //
    //     lsize_t v4( std::move(v3) );
    //     printf("v4 %p\n",v4.data());
    //
    //     printf("v1 %lu\n",v1.size());
    //     printf("v2 %lu\n",v2.size());
    //     printf("v3 %lu\n",v3.size());
    //     printf("v4 %lu\n",v4.size());
    //
    //     return;

    tp_start( std::thread::hardware_concurrency() );

    FunctionTests();

    tp_stop();

    // LOG_ALWAYS("%s","互いに同胞の精神をもって行動しなければならない。");
    // LOG_ALWAYS("%s","请以手足关系的精神相对待");

    LOG_ALWAYS( "Elapsed time %.6f s", sw.delta() );
}
