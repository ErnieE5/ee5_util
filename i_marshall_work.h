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

#ifndef I_MARSHALL_WORK_H_
#define I_MARSHALL_WORK_H_

namespace ee5 
{
    
//---------------------------------------------------------------------------------------------------------------------
// i_marshal_work
//
//
//
class i_marshal_work
{
private:
    virtual RC      lock()                              = 0;
    virtual void    unlock()                            = 0;
    virtual RC      get_storage(size_t size,void** p)   = 0;
    virtual RC      enqueue_work(i_marshaled_call *)    = 0;    

    // This turns non-scaler types into references in the function signature. Copy semantics
    // can be extraordinarily expensive and while we need to make a copy of the values during the
    // marshaling of the data, it would be "criminal" to make yet ANOTHER copy of the data that
    // is sitting in the tuple that acts as storage.
    //
    template<typename Arg>
    struct ref_val
    {
        typedef typename std::conditional<
        /* if */    std::is_scalar<Arg>::value,
        /* then */  Arg,
        /* else */  typename std::add_lvalue_reference<Arg>::type
        >::type value_type;
    };
    
    template<typename T>
    RC __lock_get(T** ppS)
    {
        RC rc = lock();
        
        if( rc == s_ok() )
        {
            rc = get_storage( sizeof(T), reinterpret_cast<void**>(ppS) );
        }
            
        return rc;
    }
        
public:

    template< typename B, typename M, typename ...TArgs >
    RC __enqueue(B&& binder,TArgs&&...args)
    {
        M* call = nullptr;
        
        RC rc = __lock_get<M>(&call);
        
        if( rc == s_ok() )
        {
            new(call) M( binder, args... );
            
            rc = enqueue_work(call);
            
            unlock();
        }
        
        return rc;
    }
    
    
    // Call a member function of a class in the context of a thread pool thread
    // with zero or more arguments with rvalue semantics.
    //
    template<typename O, typename ...TArgs>
    RC Async(O* pO, void (O::*pM)(typename ref_val<TArgs>::value_type...),TArgs&&...args)
    {
        using binder_t = object_method_delegate<O,void,typename ref_val<TArgs>::value_type...>;
        using method_t = marshaled_call<binder_t,typename std::remove_reference<TArgs>::type...>;

        return __enqueue<binder_t,method_t>( binder_t(pO,pM), args... );
    }
    
    // Call a member function of a class in the context of a thread pool thread
    // with zero or more arguments that are constant lvalue types
    //
    template<typename O, typename ... TArgs>
    RC Async(O* pO, void (O::*pM)(typename ref_val<TArgs>::value_type...),const TArgs&...args)
    {
        using binder_t = object_method_delegate<O,void,typename ref_val<TArgs>::value_type...>;
        using method_t = marshaled_call<binder_t,typename std::remove_reference<TArgs>::type...>;

        return __enqueue<binder_t,method_t>( binder_t(pO,pM), args... );
//         typedef object_method_delegate<O,void,typename ref_val<TArgs>::value_type...> binder;
//         typedef marshaled_call<binder,typename std::remove_reference<TArgs>::type...> call_type;
//         
//         CRR( enqueue_work( CallPtr( new call_type( binder(pO,pM), args... ) ) ) );
//         
//         return s_ok();
    }
    
    // Call any function or functor that can compile to a void(void) signature
    //
    template<typename TFunction>
    RC Async( TFunction f )
    {
        using method_t = marshaled_call< std::function< void() > >;

        method_t* call = nullptr;
        
        RC rc = __lock_get<method_t>(&call);
        
        if( rc == s_ok() )
        {
            new(call) method_t( f );
            
            rc = enqueue_work(call);
            
            unlock();
        }
        
        return rc;
/*        
        
        
        CRR( enqueue_work( CallPtr( new void_void_call_type(f) ) ) );
        
        return s_ok();*/
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
    RC Async(TFunction f,TArg1 a,TArgs&&...args)
    {
        using binder_t = std::function<void(TArg1,typename ref_val<TArgs>::value_type...)>;
        using method_t = marshaled_call<binder_t,TArg1,typename std::remove_reference<TArgs>::type...>;
        
        return __enqueue<binder_t,method_t>( f, a, args... );
        
        //CRR( enqueue_work( CallPtr( new call_type( f, a, args... ) ) ) );
        
        //return s_ok();
    }
    
};

    
    
    
    
}       // namespace ee5
#endif  // I_MARSHALL_WORK_H_