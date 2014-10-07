
#pragma once

#include <gridtools.h>
#ifdef CUDA_EXAMPLE
#include <stencil-composition/backend_cuda.h>
#else
#include <stencil-composition/backend_host.h>
#endif

#include <boost/timer/timer.hpp>
#include <boost/fusion/include/make_vector.hpp>

#ifdef USE_PAPI_WRAP
#include <papi_wrap.h>
#include <papi.h>
#endif

/*
  This file shows an implementation of the "horizontal diffusion" stencil, similar to the one used in COSMO
 */

using gridtools::level;
using gridtools::arg_type;
using gridtools::range;
using gridtools::arg;

using namespace gridtools;
using namespace enumtype;

// This is the definition of the special regions in the "vertical" direction
typedef gridtools::interval<level<0,-1>, level<1,-1> > x_lap;
typedef gridtools::interval<level<0,-1>, level<1,-1> > x_flx;
typedef gridtools::interval<level<0,-1>, level<1,-1> > x_out;

typedef gridtools::interval<level<0,-2>, level<1,3> > axis;

// These are the stencil operators that compose the multistage stencil in this test
struct lap_function {
    static const int n_args = 2;
    typedef arg_type<0> out;
    typedef const arg_type<1, range<-1, 1, -1, 1> > in;
    typedef boost::mpl::vector<out, in> arg_list;

    template <typename Domain>
    GT_FUNCTION
    static void Do(Domain const & dom, x_lap) {

        dom(out()) = 3*dom(in()) -
            (dom(in( 1, 0, 0)) + dom(in( 0, 1, 0)) +
             dom(in(-1, 0, 0)) + dom(in( 0,-1, 0)));
    }
};

struct flx_function {
    static const int n_args = 3;
    typedef arg_type<0> out;
    typedef const arg_type<1, range<0, 1, 0, 0> > in;
    typedef const arg_type<2, range<0, 1, 0, 0> > lap;

    typedef boost::mpl::vector<out, in, lap> arg_list;

    template <typename Domain>
    GT_FUNCTION
    static void Do(Domain const & dom, x_flx) {

        dom(out()) = dom(lap(1,0,0))-dom(lap(0,0,0));
        if (dom(out())*(dom(in(0,1,0))-dom(in(0,0,0)))) {
            dom(out()) = 0.;
        }
    }
};

struct fly_function {
    static const int n_args = 3;
    typedef arg_type<0> out;
    typedef const arg_type<1, range<0, 0, 0, 1> > in;
    typedef const arg_type<2, range<0, 0, 0, 1> > lap;
    typedef boost::mpl::vector<out, in, lap> arg_list;

    template <typename Domain>
    GT_FUNCTION
    static void Do(Domain const & dom, x_flx) {

        dom(out()) = dom(lap(0,1,0))-dom(lap(0,0,0));
        if (dom(out())*(dom(in(0,1,0))-dom(in(0,0,0)))) {
            dom(out()) = 0.;
        }
    }
};

struct out_function {
    static const int n_args = 5;
    typedef arg_type<0> out;
    typedef const arg_type<1> in;
    typedef const arg_type<2, range<-1, 0, 0, 0> > flx;
    typedef const arg_type<3, range<0, 0, -1, 0> > fly;
    typedef const arg_type<4> coeff;
    typedef boost::mpl::vector<out,in,flx,fly,coeff> arg_list;

    template <typename Domain>
    GT_FUNCTION
    static void Do(Domain const & dom, x_out) {

        dom(out()) = dom(in()) - dom(coeff()) *
            (dom(flx()) - dom(flx( -1,0,0)) +
             dom(fly()) - dom(fly( 0,-1,0))
             );
        // printf("final dom(out()) => %e\n", dom(out()));
    }
};

/*
 * The following operators and structs are for debugging only
 */
std::ostream& operator<<(std::ostream& s, lap_function const) {
    return s << "lap_function";
}
std::ostream& operator<<(std::ostream& s, flx_function const) {
    return s << "flx_function";
}
std::ostream& operator<<(std::ostream& s, fly_function const) {
    return s << "fly_function";
}
std::ostream& operator<<(std::ostream& s, out_function const) {
    return s << "out_function";
}

void handle_error(int)
{std::cout<<"error"<<std::endl;}

bool horizontal_diffusion(int x, int y, int z) {

#ifdef USE_PAPI_WRAP
  int collector_init = pw_new_collector("Init");
  int collector_execute = pw_new_collector("Execute");
#endif

    int d1 = x;
    int d2 = y;
    int d3 = z;

#ifdef CUDA_EXAMPLE
#define BACKEND backend<Cuda, Naive >
#else
#ifdef BACKEND_BLOCK
#define BACKEND backend<Host, Block >
#else
#define BACKEND backend<Host, Naive >
#endif
#endif

    //    typedef gridtools::STORAGE<double, gridtools::layout_map<0,1,2> > storage_type;

    typedef gridtools::BACKEND::storage_type<double, gridtools::layout_map<0,1,2> >::type storage_type;
    typedef gridtools::BACKEND::temporary_storage_type<double, gridtools::layout_map<0,1,2> >::type tmp_storage_type;

     // Definition of the actual data fields that are used for input/output
    storage_type in(d1,d2,d3,-1, std::string("in"));
    storage_type out(d1,d2,d3,-7.3, std::string("out"));
    storage_type coeff(d1,d2,d3,8, std::string("coeff"));

    out.print();

    // Definition of placeholders. The order of them reflect the order the user will deal with them
    // especially the non-temporary ones, in the construction of the domain
    typedef arg<0, tmp_storage_type > p_lap;
    typedef arg<1, tmp_storage_type > p_flx;
    typedef arg<2, tmp_storage_type > p_fly;
    typedef arg<3, storage_type > p_coeff;
    typedef arg<4, storage_type > p_in;
    typedef arg<5, storage_type > p_out;

    // An array of placeholders to be passed to the domain
    // I'm using mpl::vector, but the final API should look slightly simpler
    typedef boost::mpl::vector<p_lap, p_flx, p_fly, p_coeff, p_in, p_out> arg_type_list;

    // construction of the domain. The domain is the physical domain of the problem, with all the physical fields that are used, temporary and not
    // It must be noted that the only fields to be passed to the constructor are the non-temporary.
    // The order in which they have to be passed is the order in which they appear scanning the placeholders in order. (I don't particularly like this)
    gridtools::domain_type<arg_type_list> domain
        (boost::fusion::make_vector(&coeff, &in, &out /*,&fly, &flx*/));

    // Definition of the physical dimensions of the problem.
    // The constructor takes the horizontal plane dimensions,
    // while the vertical ones are set according the the axis property soon after
    // gridtools::coordinates<axis> coords(2,d1-2,2,d2-2);
    int di[5] = {2, 2, 2, d1-2, d1};
    int dj[5] = {2, 2, 2, d2-2, d2};

    gridtools::coordinates<axis> coords(di, dj);
    coords.value_list[0] = 0;
    coords.value_list[1] = d3;

    /*
      Here we do lot of stuff
      1) We pass to the intermediate representation ::run function the description
      of the stencil, which is a multi-stage stencil (mss)
      The mss includes (in order of execution) a laplacian, two fluxes which are independent
      and a final step that is the out_function
      2) The logical physical domain with the fields to use
      3) The actual domain dimensions
     */
    // gridtools::intermediate::run<gridtools::BACKEND>
    //     (
    //      gridtools::make_mss
    //      (
    //       gridtools::execute_upward,
    //       gridtools::make_esf<lap_function>(p_lap(), p_in()),
    //       gridtools::make_independent
    //       (
    //        gridtools::make_esf<flx_function>(p_flx(), p_in(), p_lap()),
    //        gridtools::make_esf<fly_function>(p_fly(), p_in(), p_lap())
    //        ),
    //       gridtools::make_esf<out_function>(p_out(), p_in(), p_flx(), p_fly(), p_coeff())
    //       ),
    //      domain, coords);

#ifdef USE_PAPI
int event_set = PAPI_NULL;
int retval;
long long values[1] = {-1};


/* Initialize the PAPI library */
retval = PAPI_library_init(PAPI_VER_CURRENT);
if (retval != PAPI_VER_CURRENT) {
  fprintf(stderr, "PAPI library init error!\n");
  exit(1);
}

if( PAPI_create_eventset(&event_set) != PAPI_OK)
    handle_error(1);
if( PAPI_add_event(event_set, PAPI_FP_INS) != PAPI_OK) //floating point operations
    handle_error(1);
#endif

#ifdef USE_PAPI_WRAP
    pw_start_collector(collector_init);
#endif

// \todo simplify the following using the auto keyword from C++11
#ifdef __CUDACC__
    gridtools::computation* horizontal_diffusion =
#else
        boost::shared_ptr<gridtools::computation> horizontal_diffusion =
#endif
        gridtools::make_computation<gridtools::BACKEND>
        (
            gridtools::make_mss // mss_descriptor
            (
                execute<parallel>(),
                gridtools::make_esf<lap_function>(p_lap(), p_in()), // esf_descriptor
                gridtools::make_independent // independent_esf
                (
                    gridtools::make_esf<flx_function>(p_flx(), p_in(), p_lap()),
                    gridtools::make_esf<fly_function>(p_fly(), p_in(), p_lap())
                    ),
                gridtools::make_esf<out_function>(p_out(), p_in(), p_flx(), p_fly(), p_coeff())
                ),
            domain, coords
            );

    horizontal_diffusion->ready();

    // domain.storage_info<boost::mpl::int_<0> >();
    // domain.storage_info<boost::mpl::int_<1> >();
    // domain.storage_info<boost::mpl::int_<2> >();
    domain.storage_info<boost::mpl::int_<3> >();
    domain.storage_info<boost::mpl::int_<4> >();
    domain.storage_info<boost::mpl::int_<5> >();

    horizontal_diffusion->steady();
    domain.clone_to_gpu();

#ifdef USE_PAPI_WRAP
    pw_stop_collector(collector_init);
#endif

    boost::timer::cpu_timer time;
#ifdef USE_PAPI
if( PAPI_start(event_set) != PAPI_OK)
    handle_error(1);
#endif
#ifdef USE_PAPI_WRAP
    pw_start_collector(collector_execute);
#endif
    horizontal_diffusion->run();

#ifdef USE_PAPI
double dummy=0.5;
double dummy2=0.8;
if( PAPI_read(event_set, values) != PAPI_OK)
    handle_error(1);
printf("%f After reading the counters: %lld\n", dummy, values[0]);
PAPI_stop(event_set, values);
#endif
#ifdef USE_PAPI_WRAP
    pw_stop_collector(collector_execute);
#endif
    boost::timer::cpu_times lapse_time = time.elapsed();

    horizontal_diffusion->finalize();

#ifdef CUDA_EXAMPLE
    out.m_data.update_cpu();
#endif

    //    in.print();
    out.print();
    //    lap.print();

    std::cout << "TIME " << boost::timer::format(lapse_time) << std::endl;

#ifdef USE_PAPI_WRAP
    pw_print();
#endif

    return // lapse_time.wall<5000000 &&
// #ifdef USE_PAPI
//                     values[0]>1000 && //random value
// #endif
                                true;
}