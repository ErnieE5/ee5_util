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
template<typename TFunc, typename ...TArgs>
class marshaled_call : public i_marshaled_call
{
public:
    typedef marshal_delegate<TFunc, void, TArgs...> Delegate;

private:
    Delegate call;

public:

    marshaled_call( TFunc&& f, TArgs&&...args ) :
        call( std::forward<TFunc>( f ), std::forward<TArgs>( args )... )
    {
    }

    template<typename...Ta1>
    marshaled_call( TFunc&& f, Ta1...args ) :
        call( std::forward<TFunc>( f ), std::forward<Ta1>( args )... )
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
template<typename TFunc>
struct m_valid
{
    using type = typename std::conditional <(
        /* The function value must be a "class" that evaluates to a functor */
        std::is_class< typename std::remove_pointer<TFunc>::type>::value ||
        /* or a function type. */
        std::is_function< typename std::remove_pointer<TFunc>::type>::value
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



//---------------------------------------------------------------------------------------------------------------------
// marshal_work
//
//
//
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
    //  F:      Function/Functor/Lambda type to call
    //  P:      aggregate function + packaged arguments / container of known size
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
                using namespace std; // To shorten the line a bit

                // Enqueue the work to the underlying implementation. Could be a thread pool or
                // any other implementation that needs to make work happen asynchronously.
                //
                //                           The package that holds all required data placed in
                //                           a single location (no use of ::new by any of the
                //                           marshalling routines) 
                //                           |
                //                           | 
                //                           |              The function/functor that is used 
                //                           |              to produce the proper stack frame
                //                           |              when the marshalling process is 
                //                           |              ready to "run" the function.
                //                           |              |
                rc = enqueue_work( new(call) P( forward<F>( f ), forward<TArgs>( args )... ) );
                //                 |                             |
                //                 |                             Arguments marshaled to the
                //                 |                             delayed function.
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
    // Call a member function of a class in the context of an underlying implementation
    // with zero or more arguments using move semantics. Any STL container or other class that
    // implements a move constructor passed as an argument will be MOVED from the original 
    // container into the marshalling data and then moved out after the marshalling has taken
    // place. 
    //      
    //      C c;   // with a method f
    //
    //      Async( &C::f, &c, std::vector<int>( { 1, 2, 3, 4, 5 } );
    //      |
    //      moved to the underlying operation (thread pool, queue, etc) then moved again
    //      |
    //      void C::f(std::vector<int> items)
    //      {
    //          // ....
    //      }
    //
    // Note: At the moment VERY LIMITED amounts of data can be marshaled as the underlying 
    //       routines are NOT COMPLETE yet. The whole POINT of move construction is to avoid having
    //       to make "deep copies" of data so this isn't a major priority at the moment.
    //
    template<typename O, typename...TArgs>
    RC Async( void ( O::*pM )( typename move_value<TArgs>::type... ), O* pO, TArgs&&...args )
    {
        using binder_t = object_method_delegate<O, void, typename move_value<TArgs>::type...>;
        using method_t = typename T::template call<binder_t, typename std::decay<TArgs>::type...>;

        return __enqueue<binder_t, method_t>( binder_t( pO, pM ), std::forward<TArgs>( args )... );
    }


    // WIP
    // 
    //  The move semantics are nice, but I would like to fully support copy semantics as well.
    //  It gets complex. I looked into using bind, but the sub-allocations that are done
    //  and the "difficulty" in obtaining the type makes it difficult. i.e. std::bind calls 
    //  ::new (or supplied allocator ~IF~ implemented!) to create storage. One of the MAJOR design
    //  elements of this whole exercise is to AVOID any additional heap use and contention on the
    //  "global" heap in the process.
    //
    template<typename O, typename...TArgs>
    RC AsyncByVal( void ( O::*pM )( typename copy_value<TArgs>::type... ), O* pO, TArgs...args )
    {
        using binder_t = object_method_delegate<O, void, typename copy_value<TArgs>::type...>;
        using method_t = typename T::template call<binder_t, typename std::decay<TArgs>::type...>;

        return __enqueue<binder_t, method_t>( binder_t( pO, pM ), std::forward<TArgs>( args )... );
    }

    // Call any function or lambda. Interestingly the captures in the list don't "really" impact 
    // the function ~signature~ and this routine still allocates the memory properly for any 
    // variables that are included in the capture.
    //
    //  Examples:
    //      struct
    //      {
    //          static void method() { };
    //      };
    //      void function() { }
    //      auto lambda = []() { };
    //      auto doit   = [](size_t x) { /*   */ };
    //
    //      Async( x::method   );
    //      Async( function    );
    //      Async( lambda      );
    //      Async( doit, 42ull );
    //
    // This version supports any lambda expression or function with arguments.
    //
    //  Notes:
    //      You are likely to encounter challenges if you try and call an operator overload on 
    //      an object via Async. (It is doable, but the syntax is hairy and it is likely easier
    //      to create a lambda expression.) While you can use std::function it is redundant 
    //      and will just add extra overhead with no gain.
    //
    // TFunc:   Any expression that evaluates to a function call or lambda expression.
    // TArgs:   Zero or more arguments
    //
    template<typename TFunc,typename...TArgs>
    typename std::enable_if< m_valid<TFunc>::value, RC>::type
    /* RC */ Async( TFunc f, TArgs&&...args )
    {
        using namespace std;
        using method_t = typename T::template call<TFunc,typename move_value<TArgs>::type...>;

        return __enqueue<TFunc, method_t>( forward<TFunc>( f ), forward<TArgs>( args )... );
    }

};


//---------------------------------------------------------------------------------------------------------------------
// marshal_work_abstract
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
