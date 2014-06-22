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

#include <cstddef>
#include <stdint.h>


BNS( ee5 )

typedef int_fast64_t 	result_code_t;
typedef result_code_t   rc_t;

#define s_ok()                  static_cast<result_code_t>(0)
#define e_out_of_memory()       static_cast<result_code_t>(0x8000000000000001)
#define e_invalid_argument(a,m) static_cast<result_code_t>(0x8000000000000002)
#define e_unexpected()          static_cast<result_code_t>(0x8000000000000003)
#define e_overflow()            static_cast<result_code_t>(0x8000000000000004)

#define e_pool_terminated()		static_cast<result_code_t>(0x8000000000000005)

#define CBREx( x, e )   do { if ( !(x) )           { return (e);                   } } while(0)
#define CMA( x )        do { if ( (x) == nullptr ) { return e_out_of_memory();     } } while(0)
#define CRR( x )        do { rc_t r=(x); if (r<0 ) { LOG_ALWAYS("Error %016llx",e_overflow()); return r;                     } } while(0)

typedef ee5::result_code_t RC;

ENS( ee5 )
