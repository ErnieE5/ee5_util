
#ifndef ERROR_HANDLING_H_
#define ERROR_HANDLING_H_

#include <cstdint>

namespace ee5
{



typedef std::int_fast64_t   result_code_t;
typedef result_code_t       rc_t;

#define s_ok()                  static_cast<result_code_t>(0)
#define e_out_of_memory()       static_cast<result_code_t>(0x8000000000000001)
#define e_invalid_argument(a,m) static_cast<result_code_t>(0x8000000000000002)
#define e_unexpected()          static_cast<result_code_t>(0x8000000000000003)

#define CBREx( x, e )   do { if ( !(x) )           { return (e);                   } } while(0)
#define CMA( x )        do { if ( (x) == nullptr ) { return e_out_of_memory();     } } while(0)
#define CRR( x )        do { rc_t r=(x); if (r<0 ) { return r;                     } } while(0)



}       // namespace oakt

typedef ee5::result_code_t RC;

#endif  // ERROR_HANDLING_H_
