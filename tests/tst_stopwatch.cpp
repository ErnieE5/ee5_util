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

#include <stopwatch.h>

#include <stdio.h>

using namespace ee5;

void tst_stopwatch()
{
    ms_stopwatch_s  _a; // A Default millisecond stopwatch with size_t values
    m_stopwatch_f   _b;
    
    float c = _a.delta<std::micro,float>();
    
    printf("A: %12lu ms\n",   _a.delta() );
    printf("B: %12.9f m\n",   _b.delta());
    printf("C: %12.3f us\n",  c);
    
    printf("B: %12.0f us\n",_b.delta<std::micro>());
    printf("B: %12.3f ms\n",_b.delta<std::milli>());
    printf("B: %12.6f s\n", _b.delta<std::ratio<1>>());
    printf("B: %12.9f m\n", _b.delta<std::ratio<60>>());
    printf("B: %12.9f h\n", _b.delta<std::ratio<3600>>());
}



