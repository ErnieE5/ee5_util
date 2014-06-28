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


template<typename TFunction, typename TReturn, typename ...TArgs>
class marshal_delegate;


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



//-------------------------------------------------------------------------------------------------
// marshal_work / i_marshal_work implementation
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
    //      This defines a type that is a template implementation that defines a type that
    //      is a pointer to a member function that is void(void)
    //
    //              void( _::* )( )
    //
    //      Then the type is used as an argument that MUST be a static type during compilation.
    //      If the second expression fails to match the signature, then SFINAE chooses the ( ... )
    //      methods. It is my hope that this name mangling is good enough!
    //
    template <typename _>
    using bc = tc < void( _::* )( ), &_::ee5__CoPieD_ByVaL__ >;

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
    using               type    = typename wrapped_type::type;
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
template<typename T, typename B = select_base<T> >
struct copy_movable : B
{
    // This is a "magic" signature to allow for testing if an object ~is~ an implementation
    // of copy_movable for argument conversion. It is never called, just declared.
    //
    void ee5__CoPieD_ByVaL__();

    // This type is the type that the signature of the TARGET call should have. The reason
    // for this is to avoid extra copies. The copied value ~stays~ in the std::tuple used to
    // marshal the data across what ever boundary (thread/queue, etc.).
    //
    using type = typename std::add_lvalue_reference<T>::type;

    copy_movable( copy_movable&& o ) : B( std::move( static_cast<B&&>(o) ) )
    {
        auto tt = typeidname< B >();
        printf("B: %s cctor\n",tt.get());
    }

    copy_movable( T&& t ) : B( std::forward<T>(t) )
    {
        auto tt = typeidname< B >();
        printf("B: %s ctor\n",tt.get());
    }

    operator B&&()
    {
        return static_cast<B&&>(*this);
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
auto byval( T i )->copy_movable<typename std::decay<T>::type>&&
{
    auto tt = typeidname< T >();
    printf("T: %s\n",tt.get());
    return std::move( copy_movable<typename std::decay<T>::type>( std::forward<T>(i) ) );
}
//
// a_sig is used to choose (adapt) the signature of the TARGET method to the appropriate type.
//
//  This has the effect of ~capturing~ most of the standard containers and moving the values
//  instead of copying. As the containers aren't thread safe (by default) this avoids extra
//  copies. If you ~don't~ want this default behavior, use the byval function to explicitly
//  declare you really WANT to make a deep copy of the object.
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

            // Acquire the memory to construct the arguments to be marshaled
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
    typename std::enable_if< m_valid<TFunc>::value, RC>::type
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
















//-------------------------------------------------------------------------------------------------
// This template acts as storage for an action that needs to be marshaled or stored for a duration.
// Once the marshaling process has taken place, the target of the action is invoked by calling the
// function operator().  Depending on how this template is constructed, the method will be invoked
// with the arguments supplied.
//
// This template ASSUMES copy semantics. The INTENTION of the class is to marshal data from the
// point of construction to where it ultimately is in a context ready for execution.
// The design is intended for threading, specifically for adding worker items to a
// thread pool.  The examples show the idea.  If you are using these objects outside the thread
// system you need to know this, otherwise most of the "goodness" happens under the covers.
//
// This template doesn't enforce what the function signature of the target should look like. Data
// marshaled is stored in a tuple by value. Complex objects should have a reference value in the
// method signature. See example 3 below.  The method signature for the object_method_delegate
// has a reference item for the vector of integers, but the argument for marshal_delegate is the
// storage type.  You CAN pass the complex object by value, but that likely to be "stupidly"
// expensive as the perfectly good copy already IN the tuple will get copied again. If you REALLY
// need to keep only one copy around, consider pointers and don't forget about locking.  (But you
// already know this anyway right?)
//
// std::bind is very close to this idea. I may switch, but I have have used T*, and T::*pmf in that
// order for going on 19 years at this point. I understand WHY the STL flipped it, but it is hard to
// adjust right now...
//
//
//  Examples:
//        #include <iostream>
//        #include <vector>
//        #include <functional>
//
//        typedef std::vector<int> int_vec;
//
//        void Foo(int a)
//        {
    //            std::cout << "Foo(" << a << ")" << std::endl;
//        }
//
//        struct Bar
//        {
    //            void Foo(int a)
//            {
    //                std::cout << "Bar::Foo(" << a << ")" << std::endl;
//            }
//            size_t Wow(int_vec& v)
//            {
    //                std::cout << "Bar::Wow(" << v.front() << ")" << std::endl;
//                return v.size();
//            }
//        };
//
//        typedef std::function< void(int) >                   F1;
//        typedef object_method_delegate<Bar,void,int>         F2;
//        typedef object_method_delegate<Bar,size_t,int_vec&>  F3;
//
//        int main()
//        {
    //            Bar b;
//
//            marshal_delegate< F1, void, int>        store1( Foo, 42 );
//            marshal_delegate< F2, void, int>        store2( F2(&b, &Bar::Foo), 42 );
//            marshal_delegate< F3, size_t, int_vec>  store3( F3(&b, &Bar::Wow), int_vec( { 42 } ) );
//
//            store1();
//            store2();
//            store3();
//        }
//
//
template<typename TFunction, typename TReturn, typename ...TArgs>
class marshal_delegate
{
public:
    static const size_t argument_count = sizeof...( TArgs );

private:
    template<size_t...>             struct sequence { };
    template<size_t N, size_t...S>  struct unpacker :unpacker < N - 1, N - 1, S... > { };
    template<size_t...S>            struct unpacker < 0, S... > { typedef sequence<S...> type; };

    using storage_t = std::tuple < typename std::decay<TArgs>::type... > ;
    using unpack_t  = typename unpacker<argument_count>::type;

    template<std::size_t N>
    using types = typename std::tuple_element<N, storage_t>::type;

    TFunction   method;
    storage_t   values;


//     template<size_t N,size_t...S>
//     struct expando : expando< N-1,N-1,S...>
//     {
//         static void doit()
//         {
//             auto tt = typeidname< types<N> >();
//             printf("expando: %s\n",tt.get());
//             printf("expando %lu ...  %i\n",N,std::conditional< is_byval<types<N>>::value, std::true_type,std::false_type>::type::value );
//         }
//
//     };
//
//     template<size_t...N>
//     struct expando<0,N...>
//     {
//         using type = typename std::conditional< is_byval<types<0>>::value, std::true_type,std::false_type>::type;
//
//         static auto doit()->typename std::conditional< is_byval<types<0>>::value, std::true_type,std::false_type>::type::value
//         {
//             auto tt = typeidname< types<0> >();
//             printf("expando: %s\n",tt.get());
//             printf("expando: %i ...  %i\n",0,std::conditional< is_byval<types<0>>::value, std::true_type,std::false_type>::type::value );
//         }
//     };
    struct moved_arg {};
    struct byval_arg {};


    template< size_t N, typename R = types<N>&& >
    R send( moved_arg )
    {
        return std::move( std::get<N>( values ) );
    }

    template< size_t N,typename R = typename is_byval<types<N>>::type& >
    R send( byval_arg )
    {
        return static_cast<R>( std::get<N>( values ) );
    }

    template<size_t N>
    using st = typename
                std::conditional<
                    is_byval<types<N>>::value,
                    byval_arg,
                    moved_arg
                >::type;

    template<size_t N>
    using ag = typename
                std::conditional<
                    is_byval<types<N>>::value,
                    typename is_byval<types<N>>::type,
                    types<N>
                >::type;



    template<size_t...S>
    TReturn tuple_call( sequence<S...> )
    {
// std::forward<ag<S>>(
        return method(  send<S>( st<S>() ) ... );
    }


public:
    template<typename F,typename...A>
    marshal_delegate( F f, A&&...args ) :
    method( std::forward<F>( f ) ),
    values( std::forward<A>( args )... )
    {
    }

    //     template<typename...Ta1>
    //     marshal_delegate( TFunction f, Ta1...args ) :
    //         method( std::forward<TFunction>(f) ),
    //         values( std::forward<Ta1>(args)...)
    //     {
        //
        //     }


    inline TReturn operator()()
    {
        return tuple_call( unpack_t() );
    }
};

ENS( ee5 )
