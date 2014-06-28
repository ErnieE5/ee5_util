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

#include <system.h>
#include <error.h>
#include <delegate.h>
#include <stdio.h>


BNS( ee5 )

//-------------------------------------------------------------------------------------------------
// marshal_work / i_marshal_work implementations
//
//


//
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
//
// Forward declaration of the marshal_delegate because it needs is_byval to operate properly
//
template<typename TFunction, typename TReturn, typename ...TArgs>
class marshal_delegate;
//
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
        call( std::forward<TFunc>( f ), args... )
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
//
// The following "mess" is all linked together to make it "simpler" to assure the intent of the
// programmer is followed but also to maintain efficiency.
//
// is_byval is a "typical" way to determine if a method exists in a template expansion. The "extra"
// weirdness is the detection of the "inner" type that is wrapped. Because of the way templates
// are expanded, all branches are evaluated. The empty struct gives a void type that is never
// used except by the initial expansions.  (Thanks internet for the basic idea!)
//
// The verbosity of template meta-programming is a bit much. I love that it enables this type of
// adaptation to the language because this code allows for "modern styles" of programming within
// the framework of an EXTRA efficient implementation. The brevity of symbols in this routine
// is NOT intended as obfuscation.
//
template<typename T>
class is_byval
{
private:
    // No real need to give analyzers any more symbols to present.
    //
    struct empty { using type = void; };
    //
    typedef char y[1]; // type with size of 1 means that the method was present
    typedef char n[2]; // 2 means that the SFINAE (Substitution Failure Is Not An Error) didn't
    //                 // find the method via the structure tc (type check)
    //
    template <typename _, _> struct tc; // Check to see if the pmf is valid
    //                 |  |
    //                 This is such a cool way to verify that one thing is like the other.
    //
    // The following expression (bc) is used twice and might make it easier to read. (Ha!)
    //
    //  Basically:
    //      bc typedefs a template implementation of tc. The first template argument is of type
    //      "pointer to a member of _" with a void(void) signature.
    //
    //              void( _::* )( )
    //
    //      Then that type is used as an argument that MUST be a static type during compilation.
    //      If the second expression fails to match the signature, then SFINAE chooses the ( ... )
    //      methods. It is my hope that this name mangling is good enough!
    //
    template <typename _>
    using bc = tc < void( _::* )( ), &_::ee5__CoPieD_ByVaL__5ee >;

    template <typename _> static n&    chk( ... );     // Nope, not a byval wrapped class
    template <typename _> static empty typ( ... );     // empty type for me

    template <typename _> static y&    chk( bc<_>* );  // Yup, byval wrapped it
    template <typename _> static T     typ( bc<_>* );  // Use the type in the wrapper.

    using decayed_type = typename std::decay<T>::type;          // The type sans any &&,&,*,const
                                                                // modifiers

    using wrapped_type = decltype( typ<decayed_type>( 0 ) );    // This is the wrapped type which
                                                                // very well may be wrapping
                                                                // itself!

    static bool const signature_exists = sizeof( chk<decayed_type>( 0 ) ) == sizeof( y );

public:
    // value is true if the tested value contains the signature false otherwise
    //
    static bool const   value   = signature_exists;

    // type is the object that is wrapped or void if testing an object that wasn't wrapped
    //
    using type     = typename wrapped_type::type;
    using type_ref = typename std::add_lvalue_reference<type>::type;
};
//
// The template expansion routines end up always expanding the arguments even if the
// evaluation path is short circuited in the conditional. (This doesn't match the semantics
// you might expect of a typical if routine. (i.e. stop evaluation if the first argument tells
// you that no more work is required.) Also, because of the recursive nature of expansion, the
// routine would end up getting wrapped in itself and we can stop that too.
//
// This allows the parser to create an acceptable base class that is never used.
//
struct copy_nothing
{
};
//
// Selector to choose the base implementation for the copy_movable adaptor
//
template<typename T>
using select_base = typename
        std::conditional<
            is_byval<T>::value || !std::is_class<T>::value,
            copy_nothing,
            T
        >::type;
//
// copy_movable is used to adapt a normally movable/moved object when you REALLY want to
// make a copy and not give up the resources.
//
template<typename T>
struct copy_movable : select_base<T>
{
    using base = select_base<T>;
    // This is a "magic" signature to allow for testing if an object ~is~ an implementation
    // of copy_movable for argument conversion. It is never called, just declared.
    //
    void ee5__CoPieD_ByVaL__5ee();

    // This type is the type that the signature of the TARGET call should have. The reason
    // for this is to avoid extra copies. The copied value ~stays~ in the std::tuple used to
    // marshal the data across what ever boundary (thread/queue, etc.).
    //
    using type = typename std::add_lvalue_reference<T>::type;

    copy_movable( copy_movable&& o ) :
        base( std::move( static_cast<base&&>(o) ) )
    {
    }

    copy_movable( T&& t ) :
        base( std::forward<T>(t) )
    {
    }
};
//
// This function allows a user of the async marshaling to make a normally movable class object
// into a copied object.  The class is used as the base of a carrier that allows the a_sig
// routines to override the default behavior of moving an object. This is useful if the INTENT
// of the async call is to use a COPY of an otherwise moveable object.
//
// Given a std::string that is a member you might not want to move it around. (I really wouldn't
// want to copy it in most cases either, but hey sometimes you have to!) So you can:
//
//      (1) Create a copy and have that copy moved to the called target.
//      (2) Or force the marshalling routines to use copy semantics.
//
//      std::string = "Hi ya!";
//
//      Async( [](std::string s)  { /*...*/ }, std::string( value ) );  // (1)
//      Async( [](std::string& s) { /*...*/ }, byval( value ) );        // (2)
//
//  More or less the above examples are identical in effect. What you choose to do should be
//  dependant on the requirements at hand. (i.e. The method is sometimes called async, but
//  most of the time it is used "inline" and a reference signature is more appropriate for the
//  nominal case.
//
template<typename T, typename std::enable_if< std::is_class<T>::value, T>::type* = nullptr >
auto byval( T i ) -> copy_movable<typename std::decay<T>::type>
{
    return copy_movable<typename std::decay<T>::type>( std::forward<T>( i ) );
}
//
// This template acts as storage for an action that needs to be marshaled or stored for a duration.
// Once the marshaling process has taken place, the target of the action is invoked by calling the
// function operator().  Depending on how this template is constructed, the method will be invoked
// with the arguments supplied.
//
// By default a complex object is MOVED into the tuple UNLESS the user elects to specifically use
// the byval() modifier function to explicitly create a copy of the object being marshaled.
//
// Complex objects that are sent byval() should have a reference value in the method signature.
//
template<typename TFunction, typename TReturn, typename ...TArgs>
class marshal_delegate
{
public:
    static const size_t argument_count = sizeof...( TArgs );

private:
    // This is a compile time unpack routine that will expand the storage tuple
    //
    template<size_t...>             struct sequence { };
    template<size_t N, size_t...S>  struct unpacker :unpacker < N - 1, N - 1, S... > { };
    template<size_t...S>            struct unpacker < 0, S... >
    {
        typedef sequence<S...> type;
    };

    // Data type of the tuple container for the marshaled data and the declaration
    // for the unpack of the arguments.
    //
    using storage_t = std::tuple < typename std::decay< TArgs >::type... >;
    using unpack_t  = typename unpacker< argument_count >::type;

    // The actual declaration of the memory layout for the storage items being marshaled
    //
    TFunction   method;
    storage_t   values;

    // A simplification of determining the types of items that are in the tuple
    //
    template< size_t N >
    using types = typename std::tuple_element< N, storage_t >::type;

    // Empty selectors that use SFINAE selection of the way the arguments need to be
    // sent to the target routine.
    //
    struct moved_a { };
    struct byval_a { };

    // Select type for forwarding if the item was captured (moved/default behavior)
    // or if the item was explicitly sent by value.
    //
    template< size_t N >
    using st = typename std::conditional< is_byval< types< N > >::value, byval_a, moved_a >::type;

    // By default the items are captured and moved if the object has a move constructor
    // defined. This is the typical selector and the items are extracted from the tuple
    // and sent to the target function.
    //
    template< size_t N>
    auto send( moved_a ) -> types<N>&&
    {
        //     Move the item OUT of the tuple to the called routine.
        //     |
        return std::move( std::get<N>( values ) );
    }

    // When the user explicitly tells us that a copy of the value should be made, and we
    // need to tell the compiler that we should send a reference to the base object to the
    // target routine.
    //
    template< size_t N>
    auto send( byval_a ) -> typename is_byval<types<N>>::type_ref
    {
        return std::get<N>( values );
    }

    // This routine is the result of a large amount of template magic that pulls all of the items
    // out of the tuple and calls the method, function, functor, or lamda captured in the method
    // with the arguments.
    //
    template<size_t...S>
    TReturn tuple_call( sequence<S...> )
    {
        //             Use the appropriate forwarding routine based on the item in the container
        //             |
        //             |        Choose the way to send the value based on the item in the tuple
        //             |        |
        return method( send<S>( st<S>() ) ... );
        //                  |      |       |
        //                  |-------       Expand the argument pack
        //                  |
        //                  S the type is a size_t value that is incremented from 0 to the
        //                  number of arguments in the tuple. Keep in mind that if the
        //                  function type is void(void) then this expression is empty.
    }

public:
    template<typename F, typename...A>
    marshal_delegate( F f, A&&...args ) :
        method( std::forward<F>( f ) ),
        values( std::forward<A>( args )... )
    {
    }

    inline TReturn operator()()
    {
        return tuple_call( unpack_t() );
    }
};
//
//  a_sig is used to choose (adapt) the signature of the TARGET method to the appropriate type.
//
//  The default has an effect of ~capturing~ most of the standard containers and moving the values
//  instead of copying. As the containers aren't thread safe (by default) this avoids extra
//  copies. A nifty implication of this is that it you keep your data in a "safely" copied
//  object that has a move constructor, only one thread can use the data at a given time. (Use
//  as a state like object.) If you ~don't~ want this default behavior, use the byval() function
//  to explicitly declare you really WANT to make a deep copy of the object.
//
template<typename Arg>
struct a_sig
{
    // Helpers to make this slightly less verbose
    //
    using decay = typename std::decay<Arg>::type;
    using lvalr = typename std::add_lvalue_reference<Arg>::type;

    //  If the Arg is a byval wrapper: Expand, then use the type
    //  of the wrapped item (a reference to the object in the called target).
    //
    //      It the object is movable or a scalar value, the signature is expected
    //      to match the "base" object.
    //
    //      Otherwise, if we can not move it then a copy will be made and the
    //      behavior is to require a reference value in the target to avoid an additional
    //      copy during invocation.
    //
    using type  = typename std::conditional<
            /* if   */  is_byval<Arg>::value,
            /* then */  typename is_byval<Arg>::type,
            /* else */  typename std::conditional<
                /* if   */  std::is_scalar<decay>::value ||
                            std::is_move_constructible<decay>::value,
                /* then */  decay,
                /* else */  lvalr
                        >::type
                  >::type;
};
//
// Helper to make the support of lambda expressions a little less hideous (IMO).
//
template<typename F>
struct f_valid
{
    using notptr = typename std::remove_pointer<F>::type;

    using type = typename
        std::conditional <
        /* if   */ std::is_class< notptr >::value || std::is_function< notptr >::value,
        /* then */ typename std::true_type,
        /* else */ typename std::false_type
        >::type;

    static const typename type::value_type value = type::value;
};
//
// Default traits for work marshaling
//
 struct marshaled_as_interface_traits
{
    template<typename F,typename...TArgs>
    using call = marshaled_call < F, TArgs... > ;
};
//
// marshal_work is a container for the adaptors that make it much simpler to capture
// the information needed to marshal data between threads without ~having~ to create
// objects to move simple parameters. This implementation is based on many C++11 constructs
// but doesn't use a few of the "obvious" ones. std::bind, std::function are both avoided
// because of the sub allocations required. While they ~might~ have a user provided allocator
// base, it is considerably better to avoid all of that to get stuff closer in memory.
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
    //  TArgs:  Any additional arguments required to construct the package being marshaled
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

            // Acquire the memory to construct the arguments to be marshaled if the enqueue_work
            // call fails, the underlying routine is REQUIRED to reclaim the memory acquired by
            // get storage. With the variadic template support, the requirement for the allocation
            // routines to be on the other side of an abstract class is relaxed. However, this
            // marshaling front end doesn't have a performance penalty when a non-abstract base
            // class is used.  (Depending on the compiler, almost everything can get fully 
            // inlined in that case.)
            //
            rc = get_storage( sizeof( P ), reinterpret_cast<void**>( &call ) );

            if( rc == s_ok() )
            {
                using namespace std; // To shorten the line horizontally a bit. :-O

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
    //  Call a member function of a class in the context of an underlying implementation
    //  with zero or more arguments using move semantics. Any STL container or other class that
    //  implements a move constructor passed as an argument will be MOVED from the original
    //  container into the marshalling data and then moved out after the marshalling has taken
    //  place.
    //
    //      struct C
    //      {
    //          void f( std::vector<int> items ) { }
    //      } c;
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
    //  If you DON'T want to give up the container use the byval modifier function.
    //
    //      std::vector<int> container( { 1, 2, 3, 4, 5 } );
    //
    //      Async( &C::f, &c, byval(container) );
    //
    // Note: At the ~moment~ VERY LIMITED amounts of data can be marshaled as the underlying
    //       routines are **NOT COMPLETE** ~yet~. The whole POINT of move construction is to avoid
    //       having to make "deep copies" of data so this isn't a major priority for me yet.
    //
    template<typename O, typename...TArgs>
    RC Async( void ( O::*pM )( typename a_sig<TArgs>::type... ), O* pO, TArgs&&...args )
    {
        using binder_t = object_method_delegate<O, void, typename a_sig<TArgs>::type...>;
        using method_t = typename T::template call<binder_t, typename std::decay<TArgs>::type...>;

        return __enqueue<binder_t, method_t>( binder_t( pO, pM ), std::forward<TArgs>( args )... );
    }
    //
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
    // This version supports any lambda expression or function/functor with arguments.
    //
    //  Notes:
    //      You are likely to encounter challenges if you try and call an operator overload on
    //      an object via Async. (It is doable, but the syntax is hairy and it is likely easier
    //      to create a lambda expression.) While you can use std::function it is redundant
    //      and will just add extra overhead with no gain.
    //
    // TFunc    Any expression that evaluates to a function call or lambda expression.
    //          Note that this member will make a copy of some objects and forward the
    //          copy along depending on how it is used.
    //
    // TArgs    Zero or more arguments
    //
    template<typename TFunc,typename...TArgs>
    typename std::enable_if< f_valid<TFunc>::value, RC>::type
    /* RC */ Async(TFunc f, TArgs&&...args )
    {
        using namespace std;
        using method_t = typename T::template call<TFunc, typename a_sig<TArgs>::type...>;

        return __enqueue<TFunc, method_t>( forward<TFunc>( f ), forward<TArgs>( args )... );
    }
};
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
//
using i_marshal_work = marshal_work < marshal_work_abstract > ;


ENS( ee5 )
