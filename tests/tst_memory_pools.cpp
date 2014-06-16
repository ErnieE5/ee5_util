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




#include <static_memory_pool.h>

#include <array>
#include <vector>
#include <forward_list>

using namespace ee5;

void tst_static_memory_pool()
{
    typedef static_memory_pool<128,10> mem_pool;
    
    mem_pool mem;
    
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





void tst_memory_pools()
{
    tst_static_memory_pool();
}


