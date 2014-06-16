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

#ifndef EE5_STOPWATCH_H_
#define EE5_STOPWATCH_H_

#include <chrono>
#include <functional>

//-------------------------------------------------------------------------------------------------
// The C++11 chrono namespace is very complete, but tends to be highly verbose to use.
//
//  This header has a few useful declarations to help make things considerably simpler (IMO) for
//  creating timed sections of code. 
//
#if 0
void stopwatch_example()
{
    us_stopwatch_s sw; // Microseconds with a default size_t value from delta()
    
    // Do some interesting stuff...
    
    printf("\nfinal %3lu\n",sw.delta());
}
#endif    


namespace ee5
{

//-------------------------------------------------------------------------------------------------
// stopwatch_base<>
//
//  On construction the current time is captured in a time_point. Calling delta() results in the
//  difference between the start and the point when delta is called.
//
//  The template arguments to the stopwatch_base class define the default behavior of a call to
//  delta().
//
//  delta() is a template function that allows the default behavior to be adapted at the point of
//  use.
//
//  The critical part is that a time_point is captured and the complex beauty of the chrono
//  namepsace allows for a huge number of transformations.
//
//  stopwatch_base can be used directly, but this header declares a number of useful aliases below.
//  You are likely better off using std::chrono directly if you think of using this class directly.
//
//  The period and type are flipped from the std::chrono implementation of duration. This was
//  purposeful as changing the period seemed more likely than the underlying type. (Example is
//  using the base as milliseconds, but capturing delta's in microseconds.)
//
//  P: the period / ratio of each integral portion of the value returned by delta().
//  T: the type of value returned
//  C: the clock to use. the HRC clock can be an alias for one of the other clocks.
//
template<typename P,typename T,typename C = std::chrono::high_resolution_clock>
class stopwatch_base
{
private:
    typename C::time_point start; // The time_point of construction / reset.

public:
    using value_type    = T;
    using period        = P;

    stopwatch_base()
    {
        reset();
    }

    void reset()
    {
        start = C::now(); // Capture the timer value
    }
    
    template<typename DP = period,typename DT = value_type >
    DT delta()
    {
        using namespace std::chrono;
        using dur = std::chrono::duration<DT,DP>;

        //  This is the fun part (and the part that typically would be verbose)
        //
        return duration_cast< dur >( C::now() - start ).count();
        //     |              |      ^^^^^^^^^^^^^^^^   |
        //     |              |      |                  The final value.
        //     |              |      |
        //     |              |      Delta between two time_points
        //     |              |
        //     |              The conversion from time_point to the type of counter
        //     |              to use.
        //     |
        //     Cast away precision. i.e. if the base resolution of the HRC is in
        //     microseconds and the type is unsigned with a period of seconds, the
        //     values returned would be the unsigned number of seconds elapsed.
    }
    
    value_type operator()()
    {
        return delta();
    }
    
    typename C::duration duration()
    {
        return C::now() - start;
    }
    
};

// The following declarations help simplify getting the time presented in
// a way that is most useful for needs. If the operation being timed is extra
// quick microseconds are handy. If an operation is longer, seconds might be
// more directly useful. Keeping the values in a double allows for simpler
// printing of fractional elements. It is probably likely that when the
// fractional parts are more important, you would want to pick a smaller default.
//
template<typename T = double> using us_stopwatch = stopwatch_base<std::micro,       T>;
template<typename T = double> using ms_stopwatch = stopwatch_base<std::milli,       T>;
template<typename T = double> using s_stopwatch  = stopwatch_base<std::ratio<1>,    T>;
template<typename T = double> using m_stopwatch  = stopwatch_base<std::ratio<60>,   T>;
template<typename T = double> using h_stopwatch  = stopwatch_base<std::ratio<3600>, T>;

// a "few" useful definitions
//
//  This code was tested on 64 bit Linux running on an Intel I7.
//  The precision of the HPC will vary on other platforms. The
//  examples of the format (and the predefined values) have
//  microseconds as the highest resolution.
//
//
//  prefix       time base
//  ----------------------------
//  us_          microseconds
//  ms_          milliseconds
//  s_           seconds
//  m_           minutes
//  h_           hours                          example
//                                              printf format's
//                                              -------------
using us_stopwatch_f    = us_stopwatch<float>;  // printf("%.0f",  t.delta() );
using us_stopwatch_d    = us_stopwatch<double>; // printf("%.0lf", t.delta() );
using us_stopwatch_s    = us_stopwatch<size_t>; // printf("%lu",   t.delta() );

using ms_stopwatch_f    = ms_stopwatch<float>;  // printf("%.3f",  t.delta() );
using ms_stopwatch_d    = ms_stopwatch<double>; // printf("%.3lf", t.delta() );
using ms_stopwatch_s    = ms_stopwatch<size_t>; // printf("%lu",   t.delta() );

using s_stopwatch_f     = s_stopwatch<float>;   // printf("%.6f",  t.delta() );
using s_stopwatch_d     = s_stopwatch<double>;  // printf("%.6lf", t.delta() );
using s_stopwatch_s     = s_stopwatch<size_t>;  // printf("%lu",   t.delta() );

using m_stopwatch_f     = m_stopwatch<float>;   // %.0f
using m_stopwatch_d     = m_stopwatch<double>;  // %.0lf
using m_stopwatch_s     = m_stopwatch<size_t>;  // printf("%lu",   t.delta() );

using h_stopwatch_f     = h_stopwatch<float>;   // %.0f
using h_stopwatch_d     = h_stopwatch<double>;  // %.0lf
using h_stopwatch_s     = h_stopwatch<size_t>;  // printf("%lu",   t.delta() );

}       // namespace ee5
#endif  // EE5_STOPWATCH_H_

