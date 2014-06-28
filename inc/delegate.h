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

#include <tuple>


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
    object_method_delegate() = delete;
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


// This is just a skeleton of an idea. I dislike heavy macro usage, but a large amount 
// of the "hate" of macros has been transferred to template meta programming at the moment! :-O
//
template<typename F, F method, typename O, typename...TArgs>
class object_instance_binder
{
private:
    O* object;

public:
    using return_type = decltype( ( std::declval<O>().*method )( std::declval<TArgs>()... ) );

    object_instance_binder() = delete;

    object_instance_binder( O* o ) :
        object( o )
    {
    }

    object_instance_binder( const object_instance_binder& _o ) :
        object( _o.object )
    {
    }

    return_type operator()( TArgs&&...args )
    {
        return ( object->*method )( std::move( args )... );
    }
};

#define instance_binder(O,M,...) \
    object_instance_binder< decltype(&O::M), &O::M, O, ##__VA_ARGS__ >





ENS( ee5 )
