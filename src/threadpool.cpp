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


#include "threadpool.h"
#include <cstddef>

#ifdef _MSC_VER
#define thread_local __declspec(thread)
#endif

#include <stdio.h>

void fart()
{
	printf("poot\n");
}

static size_t thread_local the_thread_id;



namespace ee5
{

//__attribute__ ((visibility ("external")))
size_t  work_thread_id()
{
    return the_thread_id;
}

void   set_id(size_t i)
{
	the_thread_id = i;
}


}