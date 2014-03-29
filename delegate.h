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
#ifndef EE5_DELEGATE_H_
#define EE5_DELEGATE_H_

#include <tuple>
#include <functional>


namespace ee5
{

//-------------------------------------------------------------------------------------------------
// This template binds a pointer to an object + a pointer to a member function within the
// object into a single entity that acts like a function call.
//
//
template<typename TObject,typename TReturn,typename...TArgs>
class object_method_delegate
{
public:
    typedef TObject*            PObject;
    typedef TReturn (TObject::* PMethod )(TArgs...);

private:
    PObject object;
    PMethod method;

protected:
public:
    object_method_delegate(TObject* pO,PMethod pM) :
        object(pO),
        method(pM)
    {
    }

    object_method_delegate(object_method_delegate&& _o) :
        object(_o.object),
        method(_o.method)
    {
    }

    object_method_delegate(const object_method_delegate& _o) :
        object(_o.object),
        method(_o.method)
    {
    }

    TReturn operator()(TArgs...args)
    {
        return (object->*method)(args...);
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
//
template<typename TFunction, typename TReturn, typename ...TArgs>
class marshal_delegate
{
private:

    typedef std::tuple<typename std::remove_reference<TArgs>::type...> Storage;

public:
    static const size_t argument_count = sizeof...(TArgs);

    // If this assert fires, you should likely re-think your argument passing strategy.
    //
    static_assert(argument_count<=9,"This template supports marshaling of up to 9 arguments.");

private:
    TFunction   method;
    Storage     values;

    template<size_t i> typename std::enable_if<i==0,TReturn>::type tuple_call()
    {
        return method();
    }

    template<size_t i> typename std::enable_if<i==1,TReturn>::type tuple_call()
    {
        return method(std::get<0>(values));
    }

    template<size_t i> typename std::enable_if<i==2,TReturn>::type tuple_call()
    {
        return method(std::get<0>(values),std::get<1>(values));
    }

    template<size_t i> typename std::enable_if<i==3,TReturn>::type tuple_call()
    {
        return method(std::get<0>(values),std::get<1>(values),std::get<2>(values));
    }

    template<size_t i> typename std::enable_if<i==4,TReturn>::type tuple_call()
    {
        return 
            method
            (
                std::get<0>(values),
                std::get<1>(values),
                std::get<2>(values),
                std::get<3>(values)
            );
    }

    template<size_t i> typename std::enable_if<i==5,TReturn>::type tuple_call()
    {
        return 
            method
            (
                std::get<0>(values),
                std::get<1>(values),
                std::get<2>(values),
                std::get<3>(values),
                std::get<4>(values)
            );
    }

    template<size_t i> typename std::enable_if<i==6,TReturn>::type tuple_call()
    {
        return 
            method
            (
                std::get<0>(values),
                std::get<1>(values),
                std::get<2>(values),
                std::get<3>(values),
                std::get<4>(values),
                std::get<5>(values)
            );
    }

    template<size_t i> typename std::enable_if<i==7,TReturn>::type tuple_call()
    {
        return 
            method
            (
                std::get<0>(values),
                std::get<1>(values),
                std::get<2>(values),
                std::get<3>(values),
                std::get<4>(values),
                std::get<5>(values),
                std::get<6>(values)
            );
    }

    template<size_t i> typename std::enable_if<i==8,TReturn>::type tuple_call()
    {
        return 
            method
            (
                std::get<0>(values),
                std::get<1>(values),
                std::get<2>(values),
                std::get<3>(values),
                std::get<4>(values),
                std::get<5>(values),
                std::get<6>(values),
                std::get<7>(values)
            );
    }

    template<size_t i> typename std::enable_if<i==9,TReturn>::type tuple_call()
    {
        return 
            method
            (
                std::get<0>(values),
                std::get<1>(values),
                std::get<2>(values),
                std::get<3>(values),
                std::get<4>(values),
                std::get<5>(values),
                std::get<6>(values),
                std::get<7>(values),
                std::get<8>(values)
            );
    }

public:
    marshal_delegate(TFunction f) : method( f ) // No Values to pass
    {
    }

    marshal_delegate(TFunction f,TArgs&&...args ) :
        method( f ), values( std::forward<TArgs>(args)... )
    {
    }

    marshal_delegate(TFunction f,const TArgs&...args ) :
        method( f ), values( args... )
    {
    }

    marshal_delegate(const marshal_delegate& _o) :
        method(_o.method), values(_o.values)
    {
    }

    inline TReturn operator()()
    {
        return tuple_call<argument_count>();
    }
};



//-------------------------------------------------------------------------------------------------
// This is an "interface" class. That allows pretty much anything that implements the Execute
// method be queued and generically executed.  The primary use is by the various instantiations
// of the marshaled_call template below.
//
struct i_marshaled_call
{
    virtual void Execute() = 0;
    virtual ~i_marshaled_call() { }
};



//-------------------------------------------------------------------------------------------------
// This is a slim wrapper around a marshal_delegate that adds the "overhead" of a v-table and
// the extra indirection required for the abstraction of any call into void(void). It would
// be possible to make this a base class and use multiple inheritance, but the clang front end
// and LLVM back-end do an exceptional job of making most of this disappear.
//
template<typename TFunction,typename ...TArgs>
class marshaled_call : public i_marshaled_call
{
public:
    typedef marshal_delegate<TFunction, void, TArgs...> Delegate;

private:
    Delegate call;

public:
    marshaled_call( TFunction f,TArgs&&...args) : call( f, std::forward<TArgs>(args)... )
    {
    }

    marshaled_call(TFunction f, const TArgs&...args) : call( f,  args... )
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
// Specialization for the "void f(void)" case of either a standard C function OR
// a void lambda. []{ }
//
// If you use the void lambda and have captures, be aware of the lifetime of the variables.
// Reference captures in a class may have a lifetime that stays around, but in a function it is
// HIGHLY unlikely that a reference will remain valid by the time the lambda is executed when used
// to queue stuff on another thread.  Manage the object lifetime accordingly! If you come from a
// C#/Java background you will have some very unpleasant surprises if you don't!
//
// The specialization exists to save a bit of storage and because of "interesting" ambiguity
// that occurs when you have a zero length argument pack.
//
template<>
class marshaled_call< std::function< void() > > : public i_marshaled_call
{
private:
    std::function< void() > call;
protected:
public:
    marshaled_call( const std::function< void() >& c) : call(c) {}
    ~marshaled_call() {}

    virtual void Execute()
    {
        call();
    }
};


}       // namespace ee5
#endif  // EE5_DELEGATE_H_
