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


#include "system.h"
#include "console_logger.h"


#include <atomic>

BNS( ee5 )

program_log __ee5_log = nullptr;



//---------------------------------------------------------------------------------------------------------------------
//
//
//
//
int Startup(size_t c,const char* s)
{
    static std::atomic_ulong cStartCount;

    if( ++cStartCount == 1 )
    {
        ConsoleLogger::Startup(&__ee5_log);
    }

    return s_ok();
}



//---------------------------------------------------------------------------------------------------------------------
//
//
//
//
void Shutdown()
{
    ConsoleLogger::Shutdown();
}


ENS( ee5 )

