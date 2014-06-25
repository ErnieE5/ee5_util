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
#include <delegate.h>

BNS( ee5 )


//-------------------------------------------------------------------------------------------------
// This is an "interface" class. That allows pretty much anything that implements the Execute
// method be queued and generically executed.  The primary use is by the various instantiations
// of the marshaled_call template below.
//

struct i_marshaled_call
{
    virtual void Execute() = 0;
    virtual ~i_marshaled_call()
    {
    }
};



//-------------------------------------------------------------------------------------------------
// This is a slim wrapper around a marshal_delegate that adds the "overhead" of a v-table and
// the extra indirection required for the abstraction of any call into void(void). It would
// be possible to make this a base class and use multiple inheritance, but the clang front end
// and LLVM back-end do an exceptional job of making most of this disappear.
//
template<typename TFunction, typename ...TArgs>
class marshaled_call : public i_marshaled_call
{
public:
    typedef marshal_delegate<TFunction, void, TArgs...> Delegate;

private:
    Delegate call;

public:

    marshaled_call( TFunction&& f, TArgs&&...args ) :
        call( std::forward<TFunction>( f ), std::forward<TArgs>( args )... )
    {
    }

    template<typename...Ta1>
    marshaled_call( TFunction&& f, Ta1...args ) :
        call( std::forward<TFunction>( f ), std::forward<Ta1>( args )... )
    {
    }

    ~marshaled_call()
    {
    }

    virtual void Execute()
    {
        call();
    }
};




//-------------------------------------------------------------------------------------------------
// Specialization for the "void f(void)" case of a standard C function
//
template<>
class marshaled_call< void( *)( void ) > : public i_marshaled_call
{
private:
    using void_void = void( *)( );

    void_void call;

public:
    marshaled_call( const void_void& c ) : call( c )
    {
    }
    ~marshaled_call()
    {
    }

    virtual void Execute()
    {
        call();
    }
};



// Used to move values that are scalar that have RValue constructors.
//
//  This has the effect of ~capturing~ most of the standard containers
//  and moving the values instead of copying. As the containers aren't 
//  thread safe (by default) this avoids extra copies. If you 
//  ~don't~ want this default behavior, you will need to use the ByVal
//  methods.
//
template<typename Arg>
struct move_value
{
    using type = typename std::conditional<
        /* if */    std::is_scalar<             typename std::decay<Arg>::type >::value || 
                    std::is_move_constructible< typename std::decay<Arg>::type >::value,
        /* then */  typename std::decay<Arg>::type,
        /* else */  typename std::add_lvalue_reference<Arg>::type
    >::type;
};


// This turns non-scaler types into references in the function signature. Copy semantics
// can be extraordinarily expensive and while we need to make a copy of the values during the
// marshaling of the data, it would be "criminal" to make yet ANOTHER copy of the data that
// is sitting in the tuple that acts as storage.
//
template<typename Arg>
struct copy_value
{
    using type = typename std::conditional<
        /* if */    std::is_scalar<Arg>::value,
        /* then */  Arg,
        /* else */  typename std::add_lvalue_reference<Arg>::type
    >::type;
};


// Helper to make the support of lambda expressions a little less hideous (IMO).
// 
template<typename TFunction,typename TArg1>
struct m_valid
{
    using type = typename std::conditional <(
        /* The function value must be a "class" that evaluates to a functor */
        std::is_class< typename std::remove_pointer<TFunction>::type>::value ||
        /* or a function type. */
        std::is_function< typename std::remove_pointer<TFunction>::type>::value
        ),
        typename std::true_type,
        typename std::false_type >::type;

    static const typename type::value_type value = type::value;
};


struct marshaled_as_interface_traits
{
    template<typename F,typename...TArgs>
    using call = marshaled_call < F, TArgs... > ;
};


template<typename B, typename T = marshaled_as_interface_traits>
class marshal_work : public B
{
private:
    using B::lock;
    using B::unlock;
    using B::get_storage;
    using B::enqueue_work;

    // Get Storage, Construct in place, and queue the result
    //
    //  F:      Function/Functor type to call with packaged arguments
    //  P:      aggregate function + packaged arguments container of known size
    //  TArgs:  Any additional arguments required to construct the package
    //
    template< typename F, typename P, typename...TArgs >
    RC __enqueue( F&& f, TArgs&&...args )
    {
        RC rc = e_pool_terminated();

        // Assure that the underlying object is "running." 
        //
        if( lock() )
        {
            P* call = nullptr;
        
            // Acquire the memory to construct the arguments to be marshaled
            //
            rc = get_storage( sizeof( P ), reinterpret_cast<void**>( &call ) );

            if( rc == s_ok() )
            {
                // Enqueue the work to the underlying implementation. Could be a thread pool or
                // any other implementation that needs to make work happen asynchronously.
                //
                //                           The package that holds all required data placed in
                //                           a single location (no use of ::new by any of the
                //                           marshalling routines) 
                //                           |
                //                           | 
                //                           |                   The function/functor that is used 
                //                           |                   to produce the proper stack frame
                //                           |                   when the marshalling process is 
                //                           |                   ready to "run" the function.
                //                           |                   |
                rc = enqueue_work( new(call) P( std::forward<F>( f ), std::forward<TArgs>( args )... ) );
                //                 |                                  |
                //                 |                                  Arguments marshaled to the
                //                 |                                  delayed function.
                //                 |
                //                 placement syntax new is needed to make certain that any v-table
                //                 or "other magic" is constructed along with the binder object.
                //                 placement new can not fail, however the objects that are under
                //                 construction CAN throw. The construction has to be done here
                //                 because of the requirement that an interface style object 
                //                 implement the storage.
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
    RC Async( void ( O::*pM )( typename move_value<TArgs>::type... ), O* pO, TArgs&&...args )
    {
        using binder_t = object_method_delegate<O, void, typename move_value<TArgs>::type...>;
        using method_t = typename T::template call<binder_t, typename std::decay<TArgs>::type...>;

        return __enqueue<binder_t, method_t>( binder_t( pO, pM ), std::forward<TArgs>( args )... );
    }


    // 
    // 
    //
    template<typename O, typename...TArgs>
    RC AsyncByVal( void ( O::*pM )( typename copy_value<TArgs>::type... ), O* pO, TArgs...args )
    {
        using binder_t = object_method_delegate<O, void, typename copy_value<TArgs>::type...>;
        using method_t = typename T::template call<binder_t, typename std::decay<TArgs>::type...>;

        return __enqueue<binder_t, method_t>( binder_t( pO, pM ), std::forward<TArgs>( args )... );
    }

    // Call any function or lambda that compiles to a void(void) signature.
    //
    //  Examples:
    //      struct
    //      {
    //          static void method() { };
    //      };
    //      void function() { }
    //      auto lambda = []() { };
    //
    template<typename TFunction>
    RC Async( TFunction f )
    {
        using method_t = typename T::template call< TFunction >;

        return __enqueue<TFunction, method_t>( std::forward<TFunction>( f ) );
    }


    // This is a little of a mess. In order to support lambda expressions we need to distinguish 
    // between the pointer to a member function and the first argument of the lambda.
    // This has the "messed up" side effect that a lambda expression can not pass a pointer to a
    // member function as the first argument.  This restriction is because the compiler won't be 
    // able to disambiguate between this use and the O*->Member(...) usage.
    //
    // TFunction:   Any expression that evaluates to a "plain" function call with at
    //              least a single argument.
    //
    // TArg1:       The first argument to allow for disabling selection. If there are no 
    //              arguments the above will resolve.
    //
    // TArgs:       Zero or more additional arguments
    //
    //
    //
    template<typename TFunction,typename TArg1,typename...TArgs>
    typename std::enable_if< m_valid<TFunction,TArg1>::value, RC>::type
    /* RC */ Async( TFunction f, TArg1&& a, TArgs&&...args )
    {
        using method_t = typename T::template call<TFunction, typename move_value<TArg1>::type, typename move_value<TArgs>::type...>;

        return __enqueue<TFunction, method_t>( std::forward<TFunction>( f ), std::forward<TArg1>( a ), std::forward<TArgs>( args )... );
    }

};






//---------------------------------------------------------------------------------------------------------------------
// i_marshal_work
//
//
//
class marshal_work_abstract
{
protected:
    virtual bool    lock()                                  = 0;
    virtual void    unlock()                                = 0;
    virtual RC      get_storage(size_t size,void** data)    = 0;
    virtual RC      enqueue_work(i_marshaled_call *)        = 0;
};

using i_marshal_work = marshal_work < marshal_work_abstract > ;



ENS( ee5 )
