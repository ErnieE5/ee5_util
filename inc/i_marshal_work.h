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
#include <i_marshaled_call.h>
#include <delegate.h>

BNS( ee5 )



// This turns non-scaler types into references in the function signature. Copy semantics
// can be extraordinarily expensive and while we need to make a copy of the values during the
// marshaling of the data, it would be "criminal" to make yet ANOTHER copy of the data that
// is sitting in the tuple that acts as storage.
//
template<typename Arg>
struct ref_val
{
    using type = typename std::conditional<
        /* if */    std::is_scalar<             typename std::decay<Arg>::type >::value || 
                    std::is_same<size_t,        typename std::decay<Arg>::type >::value ||
                    std::is_move_constructible< typename std::decay<Arg>::type >::value,
        /* then */  typename std::decay<Arg>::type,
        /* else */  typename std::add_lvalue_reference<Arg>::type
    >::type;
};


//---------------------------------------------------------------------------------------------------------------------
// i_marshal_work
//
//
//
class i_marshal_work
{
private:
    virtual bool    lock()                                  = 0;
    virtual void    unlock()                                = 0;
    virtual RC      get_storage(size_t* size,void** data)   = 0;
    virtual RC      enqueue_work(i_marshaled_call *)        = 0;

    template< typename B, typename M, typename...TArgs >
    RC __enqueue(B&& binder,TArgs&&...args)
    {
        RC   rc     = e_pool_terminated();
        M*   call   = nullptr;

        if( lock() )
        {
            size_t size = sizeof( M );

            rc = get_storage( &size, reinterpret_cast<void**>(&call) );

            if( rc == s_ok() )
            {
                rc = enqueue_work( new(call) M( std::forward<B>(binder), std::forward<TArgs>(args)... ) );
            }

            unlock();
        }

        return rc;
    }

public:
    // Call a member function of a class in the context of a thread pool thread
    // with zero or more arguments with rvalue semantics.
    //
    template<typename O, typename...TArgs>
    RC Async(O* pO, void (O::*pM)(typename ref_val<TArgs>::type...),TArgs&&...args)
    {
        using binder_t = object_method_delegate<O,void,typename ref_val<TArgs>::type...>;
        using method_t = marshaled_call<binder_t,typename std::decay<TArgs>::type...>;

        return __enqueue<binder_t,method_t>( binder_t(pO,pM), std::forward<TArgs>(args)... );
    }
    
    
    // Call a member function of a class in the context of a thread pool thread
    // with zero or more arguments that are constant lvalue types
    //
    template<typename O, typename...TArgs>
    RC Async( O* pO, void ( O::*pM )( typename ref_val<TArgs>::type... ),const TArgs&...args )
    {
        using binder_t = object_method_delegate<O,void,typename ref_val<TArgs>::type...>;
        using method_t = marshaled_call<binder_t,typename std::decay<TArgs>::type...>;

        return __enqueue<binder_t,method_t>( binder_t(pO,pM), std::forward<TArgs>(args)... );
    }

    // Call any function or functor that compiles to a void(void) signature
    //
    template<typename TFunction>
    RC Async( TFunction f )
    {
        using method_t = marshaled_call < TFunction > ;

        return __enqueue<TFunction, method_t>( std::forward<TFunction>(f) );
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
    RC Async(TFunction f,TArg1&& a,TArgs&&...args)
    {
        using method_t = marshaled_call<TFunction, typename ref_val<TArg1>::type, typename ref_val<TArgs>::type...>;

        return __enqueue<TFunction, method_t>( std::forward<TFunction>( f ), std::forward<TArg1>( a ), std::forward<TArgs>( args )... );
    }

};


ENS( ee5 )
