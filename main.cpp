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
class Timer
{
private:
    typedef std::chrono::time_point<std::chrono::high_resolution_clock> hrc_time_point;
    hrc_time_point s;

protected:

public:
    Timer()
    {
        s = std::chrono::high_resolution_clock::now();
    }

    template< typename D = std::chrono::duration< float, std::micro > >
    D Delta()
    {
        return std::chrono::high_resolution_clock::now() - s;
    }

    template<typename T = std::ostream,typename M = std::chrono::duration<float> >
    void PrintDelta(T& o = std::cout)
    {
//        std::streamsize p = std::cout.precision(4);
//        o << std::chrono::duration_cast<M>(Delta()).count() << std::endl;
//        std::cout.precision(p);
    }
};



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

    void CalcFactorial(unsigned long n)
    {
        LOG_FRAME(0,"n: %2lu value: %26lg",n, Factorial(n) );
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
class i_marshal_work
{

private:
    typedef std::unique_ptr<ee5::i_marshaled_call> CallPtr;

    virtual RC get_storage(size_t size,void** p)    = 0;
    virtual RC enqueue_work(CallPtr&& p)            = 0;    

//    static void* operator new(std::size_t) { return nullptr; }

public:
    // This turns non-scaler types into references in the function signature. Copy semantics
    // can be extraordinarily expensive and while we need to make a copy of the values during the
    // marshaling of the data, it would be "criminal" to make yet ANOTHER copy of the data that
    // is sitting in the tuple that acts as storage.
    //
    template<typename Arg>
    struct ref_val
    {
        typedef typename std::conditional<
                /* if */    std::is_scalar<Arg>::value,
                /* then */  Arg,
                /* else */  typename std::add_lvalue_reference<Arg>::type
                >::type value_type;
    };


    // Call a member function of a class in the context of a thread pool thread
    // with zero or more arguments with rvalue semantics.
    //
    template<typename O, typename ...TArgs>
    RC Async(O* pO, void (O::*pM)(typename ref_val<TArgs>::value_type...),TArgs&&...args)
    {
        typedef object_method_delegate<O,void,typename ref_val<TArgs>::value_type...> binder;
        typedef marshaled_call<binder,typename std::remove_reference<TArgs>::type...> call_type;

        CRR( enqueue_work( CallPtr( new call_type( binder(pO,pM), args... ) ) ) );

        return s_ok();
    }

    // Call a member function of a class in the context of a thread pool thread
    // with zero or more arguments that are constant lvalue types
    //
    template<typename O, typename ... TArgs>
    RC Async(O* pO, void (O::*pM)(typename ref_val<TArgs>::value_type...),const TArgs&...args)
    {
        typedef object_method_delegate<O,void,typename ref_val<TArgs>::value_type...> binder;
        typedef marshaled_call<binder,typename std::remove_reference<TArgs>::type...> call_type;

        CRR( enqueue_work( CallPtr( new call_type( binder(pO,pM), args... ) ) ) );

        return s_ok();
    }

    // Call any function or functor that can compile to a void(void) signature
    //
    template<typename TFunction>
    RC Async( TFunction f )
    {
        typedef marshaled_call< std::function< void() > > void_void_call_type;

        CRR( enqueue_work( CallPtr( new void_void_call_type(f) ) ) );

        return s_ok();
    }


    // This is a little of a mess. In order to support lambda expressions
    // We need to distinguish between the pointer to a member function and the first argument of the lambda.
    // This has the "messed up" side effect that a lambda expression can not pass a pointer to a
    // member function as the first argument.  This restriction is because the compiler won't be able to
    // disambiguate between this use and the O*->Member(...) usage.
    //
    template<
        typename    TFunction,  // Any expression that evaluates to a "plain" function call with at
                                // least a single argument.
        typename    TArg1,      // The first argument to allow for disabling selection
        typename... TArgs,      // Zero or more additional arguments
        typename =  typename    // This syntax sucks. "Attribute mark-up" ~could~ be better.
            std::enable_if
            <
                (
                    /* The function value must be a "class" that evaluates to a functor */
                    std::is_class< typename std::remove_pointer<TFunction>::type>::value    ||
                    /* or a function type. */
                    std::is_function< typename std::remove_pointer<TFunction>::type>::value
                )
                && /* The first marshaled argument can not be a pointer to a member function of a class. */
                (
                    ! std::is_member_function_pointer<TArg1>::value
                ),
                TFunction 
            >::type 
    >
    RC Async(TFunction f,TArg1 a,TArgs&&...args)
    {
        typedef std::function<void(TArg1,typename ref_val<TArgs>::value_type...)> function_type;
        typedef marshaled_call<function_type,TArg1,typename std::remove_reference<TArgs>::type...> call_type;

        CRR( enqueue_work( CallPtr( new call_type( f, a, args... ) ) ) );

        return s_ok();
    }

};

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
    










//---------------------------------------------------------------------------------------------------------------------
//
//
//
//
class TP : public i_marshal_work
{
private:
    typedef std::unique_ptr<ee5::i_marshaled_call> CallPtr;
    typedef ee5::WorkThread<CallPtr>               work_thread;
    typedef std::vector<work_thread>               thread_vector;
    thread_vector   threads;
    size_t          t_count = std::thread::hardware_concurrency();
    size_t          x       = 0;

    RC get_storage(size_t size,void** pp)
    {
        CBREx( pp != nullptr, e_invalid_argument(2,"value must be non-null") );
        CMA( *pp = calloc(size,1) );

        return s_ok();
    }

    RC enqueue_work(CallPtr&& p)
    {
        threads[x%t_count].Enqueue( std::move( p ) , nullptr);
        ++x;
        return s_ok();
    }

public:
    TP()
    {
        // Create the threads first...
        //
        for(size_t c = t_count; c ; --c)
        {
            threads.push_back( work_thread( c, [](CallPtr& p){ p->Execute(); } ) );
        }

        // Start them.
        for(auto& s : threads)
        {
            s.Startup();
        }
    }

    void Shutdown()
    {
        for(auto& k : threads)
        {
            k.Quit();
        }
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
    TP              p;
    int             int_local       = 55555;
    double          double_local    = 55.555;

    Timer t;

    // Factorial Work Product
    //
    for(size_t n=0;n<26;++n)
    {
        CRR( p.Async( &target, &ThreadpoolTest::CalcFactorial, n ) );
    }


    // Scalar Types
    //
    CRR( p.Async( &target, &ThreadpoolTest::ScalarTypes, 1, 11.1, 1000000000000000ul ) );


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
            LOG_ALWAYS("[h]:%i", h );
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
    auto q = [h,&cc](int a,double b, int c)
    {
        long double ld = random() * b;

        for(size_t i = 0;i < 10000;i++)
        {
            ld += ThreadpoolTest::Factorial(25) * ThreadpoolTest::Factorial(25) * ThreadpoolTest::Factorial(25);
        }

        // This should never happen, but the optimizer is too good
        // and will completely compile this code away if there is
        // NOT a chance that the calculated value is never used.
        //
        if( ld == 0 )
        {
            LOG_ALWAYS("ld: %20.20lg",ld);
        }

        cc++; // The atomically incremented value
    };

    ++h;

    // Lambda capture (h) with three arguments, queued inline
    //
    CRR( p.Async( [h](int a,double b, int c)
    {
        LOG_ALWAYS("[h]:%i a:%i b:%g c:%i", h, a, b, c );
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

    // This test "adds" more time by creating a large amount of work
    // by doing "heap stuff" on a number of threads.
    //
    for(unsigned long long size = 1; size < 1000000000; size *= 10)
    {
        CRR( p.Async( [size]
        {
            typedef std::chrono::high_resolution_clock      HRC;
            typedef std::chrono::duration<float,std::milli> ms;

            auto start = HRC::now();

            std::vector<int> v( size, 42 );

            LOG_ALWAYS("Items %11lu, Time: % 12.6f ms", size,
                       std::chrono::duration_cast<ms>( HRC::now() - start ).count() );
        } ) );
    }

    p.Shutdown();

    assert( cc == 7777777 );

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
    
    Timer   taft;

    FunctionTests();
    LOG_ALWAYS("Elapsed Time: %lg seconds.",
               std::chrono::duration_cast<std::chrono::duration<float>>(taft.Delta()).count() );

    LOG_ALWAYS("%s","互いに同胞の精神をもって行動しなければならない。");
    LOG_ALWAYS("%s","请以手足关系的精神相对待");

    for(size_t y = 0;y < 10;++y)
    {
        LOG_ALWAYS("Hey %lu",y);
    }
}





//-------------------------------------------------------------------------------------------------
//
//
//
//
int main()
{ 
   Moink();
    
   int iRet = 0;
//    int iRet = ee5::Startup(0,nullptr);
// 
//     if( iRet == 0 )
//     {
//         App();
// 
//         ee5::Shutdown();
//     }


    return iRet;
}

