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

#include <stopwatch.h>
#include <spin_locking.h>
#include <i_marshal_work.h>

#include <algorithm>
#include <mutex>
#include <numeric>
#include <cstdio>
#include <vector>
#include <array>

using namespace ee5;

template<typename L,size_t iterations = 800000>
void run_lock_test(i_marshal_work* p)
{
    printf("rlt %-25.25s ",typeid(L).name()); fflush(stdout);

    m_stopwatch_f       sw;
    L                   lock;
    std::vector<size_t> times;
    std::atomic_size_t  a;
    std::atomic_size_t  c;
    long double         d = 0;
    size_t              f[100] = { };

    times.reserve(iterations);

    for(size_t x = 0;x < iterations;++x)
    {
        RC dd = s_ok();
        do
        {
            dd = p->Async( [&]()
            {
                long double ttf  = 0;

                // for(size_t q = 0;q < 100;q++)
                // {
                //     ttf += ThreadpoolTest::Factorial(q*2);
                // }

                us_stopwatch_s swt;
                lock.lock();
                times.push_back(swt.delta());

                static thread_local size_t tid = a++;

                //size_t iter = 
                c++;
                f[tid]++;
                d += ttf;

                lock.unlock();

                // for(size_t j = 0;j < 1;j++)
                // {
                //     RC qq = s_ok();

                //      do
                //      {
                //         qq = p.Async( [&]()
                //         {
                //             long double ttg = ThreadpoolTest::Factorial(25);
                //             lock.lock();
                //             d += ttg;
                //             lock.unlock();
                //         });
                //      }
                //      while( qq != s_ok() );
                // }
            });
        }
        while( dd != s_ok() );
    }

//     size_t xx;
//     do
//     {
//         xx = p.Pending();
// //        printf("%8lu %8lu\n",xx,c.load());
//     }
//     while(xx>0);

    size_t sum = std::accumulate(times.begin(),times.end(),0);
    auto mm = std::minmax_element(times.begin(),times.end());
    printf("-%10lu-",sum);

    printf("%12.8f m %9lu / ", sw.delta(), c.load());
    //printf("%20lu us %9lu / ", sw.delta(), c.load());
    // for(size_t b = 0;b < p.Count() ;b++)
    // {
    //     printf("%2lu:%9lu ",b,f[b]);
    // }
    printf("%lu us (%lu,%lu)\n",sum/iterations,*mm.first,*mm.second); fflush(stdout);
}


void tst_spin_locks()
{
    static constexpr size_t iterations = 8 * 100000000;

    i_marshal_work* p = nullptr;

    run_lock_test<std::mutex,iterations>           (p);
    run_lock_test<spin_posix,iterations>           (p);
    run_lock_test<spin_mutex,iterations>           (p);
    run_lock_test<spin_barrier,iterations>         (p);
    run_lock_test<spin_shared_mutex_t,iterations>  (p);
}