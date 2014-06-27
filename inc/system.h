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

#include <error.h>
#include <cstddef>
#include <marshaling.h>

BNS( ee5 )

int     Startup(size_t c,const char* s);
void    Shutdown();

struct async_call
{
    static i_marshal_work* tp;

    template<typename F,typename...TArgs>
    RC operator()(const F& f, TArgs&&...args )
    {
        return tp->Async( f, std::forward<TArgs>( args )... );
    }

    template<typename O, typename...TArgs>
    RC operator()( void ( O::*pM )( typename a_sig<TArgs>::type... ), O* pO, TArgs&&...args )
    {
        return tp->Async( pM, pO, std::forward<TArgs>( args )... );
    }

    operator i_marshal_work*( )
    {
        return tp;
    }
    i_marshal_work* operator->( )
    {
        return tp;
    }
};

extern async_call async;

void    tp_start( size_t c );
void    tp_stop();
size_t  tp_pending();
size_t  tp_count();
void    tp_park( size_t c );


ENS( ee5 )
