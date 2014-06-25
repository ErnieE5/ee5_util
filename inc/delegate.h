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
//
// This header contains delegate template helpers.
//
#pragma once
#include <ee5>

#include <cstddef>
#include <tuple>
#include <functional>


BNS( ee5 )

//-------------------------------------------------------------------------------------------------
// This template binds a pointer to an object + a pointer to a member function within the
// object into a single entity that acts like a function call.
//
//
template<typename TObject, typename TReturn, typename...TArgs>
class object_method_delegate
{
public:
    typedef TObject*            PObject;
    typedef TReturn( TObject::* PMethod )( TArgs... );

private:
    PObject object;
    PMethod method;

protected:
public:
    object_method_delegate( TObject* pO, PMethod pM ) :
        object( pO ),
        method( pM )
    {
    }

    object_method_delegate( object_method_delegate&& _o ) :
        object( _o.object ),
        method( _o.method )
    {
    }

    object_method_delegate( const object_method_delegate& _o ) :
        object( _o.object ),
        method( _o.method )
    {
    }

    TReturn operator()( TArgs&&...args )
    {
        return ( object->*method )( std::move( args )... );
    }
};


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
    template<int...>         struct sequence { };
    template<int N, int...S> struct unpacker :unpacker < N - 1, N - 1, S... > { };
    template<int...S>        struct unpacker < 0, S... > { typedef sequence<S...> type; };

    using storage_t = std::tuple < typename std::remove_reference<TArgs>::type... > ;
    using unpack_t  = typename unpacker<argument_count>::type;

    TFunction   method;
    storage_t   values;

    template<int...S>
    TReturn tuple_call( sequence<S...> )
    {
        return method( std::move( std::get<S>( values ) )... );
    }

public:
    marshal_delegate( TFunction f, TArgs&&...args ) :
        method( std::forward<TFunction>( f ) ), values( std::forward<TArgs>( args )... )
    {
    }

    inline TReturn operator()()
    {
        return tuple_call( unpack_t() );
    }
};





ENS( ee5 )
