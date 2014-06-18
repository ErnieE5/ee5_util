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

#include <cstdio>
#include <stopwatch.h>
#include <spin_locking.h>
#include <i_marshal_work.h>


#include <algorithm>
#include <mutex>
#include <numeric>
#include <cstdio>
#include <vector>
#include <array>
#include <unordered_map>

using namespace ee5;

extern i_marshal_work* pool;

void    tp_start(size_t);
void    tp_stop();
size_t  tp_pending();
size_t  tp_count();
void    tp_park(size_t);


static long double Factorial(size_t n,long double a = 1)
{
    if( n == 0 ) return a;
    return Factorial(n-1, a * n );
}

#ifndef _MSC_VER
#include <cxxabi.h>
#include <utility>
struct distroy_abi { void operator()(void* buffer) { free(buffer); } };
using abi_name = std::unique_ptr<char,distroy_abi>;

template<typename T>
abi_name typeidname()
{
    int status = 0;
    return abi_name( abi::__cxa_demangle(typeid(T).name(),nullptr,nullptr,&status) );
}
#else
struct abi_name
{
    const char * name;
    const char * get() { return name + 6; }
};
template<typename T> abi_name typeidname() { return { typeid(T).name() }; }
#endif

struct stats
{
    abi_name    name;
    float       total_time;
    size_t      time_stalled;
    size_t      mode_stalled;
    size_t      min_stalled;
    size_t      max_stalled;

    size_t      mode_loop;
    size_t      min_loop;
    size_t      max_loop;
    size_t      ave_loop;

    size_t      iterations;
    size_t      inner_loop;

    stats
    (
        abi_name&&      n,
        float           tt,
        size_t          ts,
        size_t          md,
        size_t          mins,
        size_t          maxs,
        size_t          modl,
        size_t          minl,
        size_t          maxl,
        size_t          avel,
        size_t          i,
        size_t          l
    ) : name(std::move(n))
    {
        total_time      = tt;
        time_stalled    = ts;
        mode_stalled    = md,
        min_stalled     = mins;
        max_stalled     = maxs;
        mode_loop       = modl;
        min_loop        = minl;
        max_loop        = maxl;
        ave_loop        = avel;
        iterations      = i;
        inner_loop      = l;
    }
};

template<typename T,typename...Comp>
void sort(T& c,Comp...comp)
{
    std::sort(c.begin(),c.end(),comp...);
}

size_t time_it( std::function<void()> f )
{
    us_stopwatch_s start;
    f();
    return start.delta();
}

namespace ee5 { size_t work_thread_id(); }
template<typename L>
stats lock_test(i_marshal_work* p,size_t iterations,size_t inner)
{
    auto name = typeidname<L>();

    printf("%s\n",name.get());
    fflush(stdout);

    using un_map = std::unordered_map<size_t,size_t>;

    std::atomic_size_t       a;
    m_stopwatch_f            sw;

    L                        lock;
    std::vector<size_t>      times;
    un_map                   mode_work_data;
    size_t                   c = 0;
    long double              d = 0;
    size_t                   f[100] = { };
    size_t                   fa = 0;
    std::pair<size_t,size_t> mmf;

    times.reserve(iterations);

    for(size_t x = 0;x < iterations;++x)
    {
        RC dd = s_ok();
        do
        {
            dd = p->Async( [&]()
            {
                us_stopwatch_s factorial_loop_timer;

                long double ttf  = 0;
                for(size_t q = 0;q < inner;q++)
                {
                    ttf += Factorial(q);
                }
                size_t fact_time = factorial_loop_timer.delta();


                times.push_back( time_it( [&] { lock.lock(); } ) );

                mode_work_data[fact_time]++;

                fa += fact_time;
                c++;
                f[work_thread_id()]++;
                d += ttf;

                mmf = std::minmax( std::initializer_list<size_t>({ mmf.first, mmf.second, fact_time }) );

                lock.unlock();
            });

            if(dd != s_ok())
            {
                tp_park(1000);
            }
        }
        while( dd != s_ok() );
    }

    while(tp_pending() > 0);

    float total_time = sw.delta();

    size_t                      sum_stall  = 0;
    size_t                      md_m = 0;
    size_t                      mode_stall_value = 0;
    size_t                      mode_work_value = 0;
    std::pair<size_t,size_t>    mm_stall;
    un_map                      mode_stall_data;
    for(size_t t : times)
    {
        sum_stall += t;
        mm_stall  = std::minmax( std::initializer_list<size_t>({ mm_stall.first, mm_stall.second, t }) );

        mode_stall_data[t]++;
    }
    for(const auto& pss : mode_stall_data)
    {
        if( std::max( md_m, pss.second ) != md_m )
        {
            md_m                = pss.second;
            mode_stall_value    = pss.first;
        }
    }
    md_m = 0;
    for(const auto& pss : mode_work_data)
    {
        if( std::max( md_m, pss.second ) != md_m )
        {
            md_m             = pss.second;
            mode_work_value  = pss.first;
        }
    }

//    printf("Hey %lu\n",__);

    return stats(   std::move(name),
                    total_time, sum_stall, mode_stall_value, mm_stall.first, mm_stall.second,
                    mode_work_value, mmf.first, mmf.second, fa/iterations, iterations, inner );
}




void tst_spin_locks()
{
    size_t concurrency  = std::thread::hardware_concurrency();
    size_t iterations   = 100000000;
    size_t work_loop    = 200;

    tp_start(concurrency);

    

    size_t x = 0;

    // tests[x++] = std::bind( lock_test<std::mutex>,          pool, iterations, work_loop );
    // tests[x++] = std::bind( lock_test<spin_native>,         pool, iterations, work_loop );
    // tests[x++] = std::bind( lock_test<spin_mutex>,          pool, iterations, work_loop );
    // tests[x++] = std::bind( lock_test<spin_barrier>,        pool, iterations, work_loop );
    // tests[x++] = std::bind( lock_test<spin_shared_mutex_t>, pool, iterations, work_loop );

    // std::random_shuffle( tests.begin(), tests.end() );

    using vs_t = std::vector<stats>;
    vs_t data;

    std::array<std::function<stats()>,5> tests;
    data.push_back( lock_test<spin_flag>(pool, iterations, work_loop) );

    for(auto& r : tests)
    {
        if (r) data.push_back( r() );
    }

    printf("%40.40s\n\n","");

    auto slt = [](const stats& a,const stats& b)->bool
    {
        return a.total_time < b.total_time;
    };

    sort( data, slt );

    printf("Concurrency:    %lu\n",concurrency);
    printf("Scheduled:      %lu\n",iterations);
    printf("Work loop:      %lu\n",work_loop);
    printf("\n");

    float base = data.back().total_time;

    printf("                                 Blocking             Working                 \n");
    printf("Barrier            total mode ave     max  mode   ave     max     Total       \n");
    printf("------------ ---------------------------- ------------------- --------- ------\n");
    for(auto& a: data)
    {
        printf("%-12.12s ",             a.name.get() +5 );
        printf("%11lu %4lu %3lu %7lu ", a.time_stalled,a.mode_stalled,a.time_stalled/a.iterations,a.max_stalled);
        printf("%5lu %5lu %7lu ",       a.mode_loop,a.ave_loop,a.max_loop);
        printf("%9.3f ",                a.total_time);
        printf("%5.2f%%\n",             100-(a.total_time/base*100));
    }
    printf("------------ ---------------------------- ------------------- --------- ------\n");
    printf("             ^^^^^^^^^^^ ^^^^ ^microseconds^^^^ ^^^^^ ^^^^^^^ ^minutes^\n\n");


    tp_stop();
}
