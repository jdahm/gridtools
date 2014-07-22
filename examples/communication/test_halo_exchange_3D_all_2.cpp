
/*
Copyright (c) 2012, MAURO BIANCO, UGO VARETTO, SWISS NATIONAL SUPERCOMPUTING CENTRE (CSCS)
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Swiss National Supercomputing Centre (CSCS) nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL MAURO BIANCO, UGO VARETTO, OR 
SWISS NATIONAL SUPERCOMPUTING CENTRE (CSCS), BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include <mpi.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <communication/halo_exchange.h>
#include <string>
#include <stdlib.h>
#include <common/layout_map.h>
#include <common/boollist.h>
#include <sys/time.h>

int pid;
int nprocs;
MPI_Comm CartComm;
int dims[3] = {0,0,0};
int coords[3]={0,0,0};

struct timeval start_tv;
struct timeval stop1_tv;
struct timeval stop2_tv;
struct timeval stop3_tv;
double lapse_time1;
double lapse_time2;
double lapse_time3;
double lapse_time4;

#define B_ADD 1
#define C_ADD 2

typedef gridtools::gcl_cpu arch_type;

template <typename T, typename lmap>
struct array {
  T *ptr;
  int n,m,l;

  array(T* _p, int _n, int _m, int _l)
    : ptr(_p)
    , n(lmap::template find<2>(_n,_m,_l))
    , m(lmap::template find<1>(_n,_m,_l))
    , l(lmap::template find<0>(_n,_m,_l))  
  {}

  T &operator()(int i, int j, int k) {
    // a[(DIM1+2*H)*(DIM2+2*H)*kk+ii*(DIM2+2*H)+jj]
    return ptr[l*m*lmap::template find<2>(i,j,k)+
               l*lmap::template find<1>(i,j,k)+
               lmap::template find<0>(i,j,k)];
  }

  T const &operator()(int i, int j, int k) const {
    return ptr[l*m*lmap::template find<2>(i,j,k)+
               l*lmap::template find<1>(i,j,k)+
               lmap::template find<0>(i,j,k)];
  }

  operator void*() const {return reinterpret_cast<void*>(ptr);}
  operator T*() const {return ptr;}
};

/** \file Example of use of halo_exchange pattern for regular
    grids. The comments in the code aim at highlight the process of
    instantiating and running a halo exchange pattern.
*/

inline int modulus(int __i, int __j) {
  return (((((__i%__j)<0)?(__j+__i%__j):(__i%__j))));
}

#include "triplet.h"

/* Just and utility to print values
 */
template <typename array_t>
void printbuff(std::ostream &file, array_t const & a, int d1, int d2, int d3) {
  if (d1<=7 && d2<=7 && d3<=7) {
    file << "------------\n";
    for (int kk=0; kk<d3; ++kk) {
      file << "|";
      for (int jj=0; jj<d2; ++jj) {
        for (int ii=0; ii<d1; ++ii) {
          file << a(ii,jj,kk);
        }
        file << "|\n";
      }
      file << "\n\n";
    }
    file << "------------\n\n";
  }
}


template <typename ST, int I1, int I2, int I3, bool per0, bool per1, bool per2>
void run(ST & file, int DIM1, int DIM2, int DIM3, int H1, int H2, int H3, triple_t<USE_DOUBLE> *_a, triple_t<USE_DOUBLE> *_b, triple_t<USE_DOUBLE> *_c) {

  typedef gridtools::layout_map<I1,I2,I3> layoutmap;
  
  array<triple_t<USE_DOUBLE>, layoutmap > a(_a, (DIM1+2*H1),(DIM2+2*H2),(DIM3+2*H3));
  array<triple_t<USE_DOUBLE>, layoutmap > b(_b, (DIM1+2*H1),(DIM2+2*H2),(DIM3+2*H3));
  array<triple_t<USE_DOUBLE>, layoutmap > c(_c, (DIM1+2*H1),(DIM2+2*H2),(DIM3+2*H3));

  /* Just an initialization */
  for (int ii=0; ii<DIM1+2*H1; ++ii)
    for (int jj=0; jj<DIM2+2*H2; ++jj) {
      for (int kk=0; kk<DIM3+2*H3; ++kk) {
        a(ii,jj,kk) = triple_t<USE_DOUBLE>();
        b(ii,jj,kk) = triple_t<USE_DOUBLE>();                                      
        c(ii,jj,kk) = triple_t<USE_DOUBLE>();
      }
    }
//   a(0,0,0) = triple_t<USE_DOUBLE>(3000+gridtools::PID, 4000+gridtools::PID, 5000+gridtools::PID);
//   b(0,0,0) = triple_t<USE_DOUBLE>(3010+gridtools::PID, 4010+gridtools::PID, 5010+gridtools::PID);
//   c(0,0,0) = triple_t<USE_DOUBLE>(3020+gridtools::PID, 4020+gridtools::PID, 5020+gridtools::PID);


  /* The pattern type is defined with the layouts, data types and
     number of dimensions.

     The logical assumption done in the program is that 'i' is the
     first dimension (rows), 'j' is the second, and 'k' is the
     third. The first layout states that 'i' is the second dimension
     in order of strides, while 'j' is the first and 'k' is the third
     (just by looking at the initialization loops this shoule be
     clear).

     The second layout states that the first dimension in data ('i')
     identify also the first dimension in the communicator. Logically,
     moving on 'i' dimension from processot (p,q,r) will lead you
     logically to processor (p+1,q,r). The other dimensions goes as
     the others.
   */
#ifndef PACKING_TYPE
#define PACKING_TYPE gridtools::version_manual
#endif

  static const int version = PACKING_TYPE;

  typedef gridtools::halo_exchange_dynamic_ut<layoutmap, 
    gridtools::layout_map<0,1,2>, triple_t<USE_DOUBLE>::data_type, 3, arch_type, version > pattern_type;

  /* The pattern is now instantiated with the periodicities and the
     communicator. The periodicity of the communicator is
     irrelevant. Setting it to be periodic is the best choice, then
     GCL can deal with any periodicity easily.
  */
  pattern_type he(typename pattern_type::grid_type:: period_type(per0, per1, per2), CartComm);


  /* Next we need to describe the data arrays in terms of halo
     descriptors (see the manual). The 'order' of registration, that
     is the index written within <.> follows the logical order of the
     application. That is, 0 is associated to 'i', '1' is
     associated to 'j', '2' to 'k'.
  */
  he.template add_halo<0>(H1, H1, H1, DIM1+H1-1, DIM1+2*H1);
  he.template add_halo<1>(H2, H2, H2, DIM2+H2-1, DIM2+2*H2);
  he.template add_halo<2>(H3, H3, H3, DIM3+H3-1, DIM3+2*H3);

  /* Pattern is set up. This must be done only once per pattern. The
     parameter must me greater or equal to the largest number of
     arrays updated in a single step.
  */
  he.setup(3);


  file << "Proc: (" << coords[0] << ", " << coords[1] << ", " << coords[2] << ")\n";


  /* Data is initialized in the inner region of size DIM1xDIM2
   */
  for (int ii=H1; ii<DIM1+H1; ++ii)
    for (int jj=H2; jj<DIM2+H2; ++jj) 
      for (int kk=H3; kk<DIM3+H3; ++kk) {
        a(ii,jj,kk) = //(100*(pid))+
          triple_t<USE_DOUBLE>(ii-H1+(DIM1)*coords[0],
                   jj-H2+(DIM2)*coords[1],
                   kk-H3+(DIM3)*coords[2]);
          b(ii,jj,kk) = //(200*(pid))+ 
          triple_t<USE_DOUBLE>(ii-H1+(DIM1)*coords[0]+B_ADD,
                   jj-H2+(DIM2)*coords[1]+B_ADD,
                   kk-H3+(DIM3)*coords[2]+B_ADD);
          c(ii,jj,kk) = //300*(pid))+
          triple_t<USE_DOUBLE>(ii-H1+(DIM1)*coords[0]+C_ADD,
                   jj-H2+(DIM2)*coords[1]+C_ADD,
                   kk-H3+(DIM3)*coords[2]+C_ADD);
      }

  printbuff(file,a, DIM1+2*H1, DIM2+2*H2, DIM3+2*H3);

  /* This is self explanatory now
   */

  std::vector<triple_t<USE_DOUBLE>::data_type*> vect(3);
  vect[0] = reinterpret_cast<triple_t<USE_DOUBLE>::data_type*>(a.ptr);
  vect[1] = reinterpret_cast<triple_t<USE_DOUBLE>::data_type*>(b.ptr);
  vect[2] = reinterpret_cast<triple_t<USE_DOUBLE>::data_type*>(c.ptr);

  // std::vector<triple_t<USE_DOUBLE>*> vect(3);
  // vect[0] = a.ptr;
  // vect[1] = b.ptr;
  // vect[2] = c.ptr;
  MPI_Barrier(gridtools::GCL_WORLD);
  
  gettimeofday(&start_tv, NULL);

  he.post_receives();
  he.pack(vect);

  //  MPI_Barrier(MPI_COMM_WORLD);
  gettimeofday(&stop1_tv, NULL);

  he.do_sends();
  // MPI_Barrier(MPI_COMM_WORLD);
  he.wait();

  //MPI_Barrier(MPI_COMM_WORLD);
  gettimeofday(&stop2_tv, NULL);

  he.unpack(vect);

  MPI_Barrier(MPI_COMM_WORLD);
  gettimeofday(&stop3_tv, NULL);

  lapse_time1 = ((static_cast<double>(stop1_tv.tv_sec)+1/1000000.0*static_cast<double>(stop1_tv.tv_usec)) - (static_cast<double>(start_tv.tv_sec)+1/1000000.0*static_cast<double>(start_tv.tv_usec))) * 1000.0;

  lapse_time2 = ((static_cast<double>(stop2_tv.tv_sec)+1/1000000.0*static_cast<double>(stop2_tv.tv_usec)) - (static_cast<double>(stop1_tv.tv_sec)+1/1000000.0*static_cast<double>(stop1_tv.tv_usec))) * 1000.0;

  lapse_time3 = ((static_cast<double>(stop3_tv.tv_sec)+1/1000000.0*static_cast<double>(stop3_tv.tv_usec)) - (static_cast<double>(stop2_tv.tv_sec)+1/1000000.0*static_cast<double>(stop2_tv.tv_usec))) * 1000.0;

  lapse_time4 = ((static_cast<double>(stop3_tv.tv_sec)+1/1000000.0*static_cast<double>(stop3_tv.tv_usec)) - (static_cast<double>(start_tv.tv_sec)+1/1000000.0*static_cast<double>(start_tv.tv_usec))) * 1000.0;

  MPI_Barrier(MPI_COMM_WORLD);
  file << "TIME PACK: " << lapse_time1 << std::endl;
  file << "TIME EXCH: " << lapse_time2 << std::endl;
  file << "TIME UNPK: " << lapse_time3 << std::endl;
  file << "TIME ALL : " << lapse_time1+lapse_time2+lapse_time3 << std::endl;
  file << "TIME TOT : " << lapse_time4 << std::endl;

  file << "\n********************************************************************************\n";

  printbuff(file,a, DIM1+2*H1, DIM2+2*H2, DIM3+2*H3);

  int passed = true;


  /* Checking the data arrived correctly in the whole region
   */
  for (int ii=0; ii<DIM1+2*H1; ++ii)
    for (int jj=0; jj<DIM2+2*H2; ++jj)
      for (int kk=0; kk<DIM3+2*H3; ++kk) {

        triple_t<USE_DOUBLE> ta;
        triple_t<USE_DOUBLE> tb;
        triple_t<USE_DOUBLE> tc;
        int tax, tay, taz;
        int tbx, tby, tbz;
        int tcx, tcy, tcz;

        tax = modulus(ii-H1+(DIM1)*coords[0], DIM1*dims[0]);
        tbx = modulus(ii-H1+(DIM1)*coords[0], DIM1*dims[0])+B_ADD;
        tcx = modulus(ii-H1+(DIM1)*coords[0], DIM1*dims[0])+C_ADD;

        tay = modulus(jj-H2+(DIM2)*coords[1], DIM2*dims[1]);
        tby = modulus(jj-H2+(DIM2)*coords[1], DIM2*dims[1])+B_ADD;
        tcy = modulus(jj-H2+(DIM2)*coords[1], DIM2*dims[1])+C_ADD;

        taz = modulus(kk-H3+(DIM3)*coords[2], DIM3*dims[2]);
        tbz = modulus(kk-H3+(DIM3)*coords[2], DIM3*dims[2])+B_ADD;
        tcz = modulus(kk-H3+(DIM3)*coords[2], DIM3*dims[2])+C_ADD;

        if (!per0) {
          if ( ((coords[0]==0) && (ii<H1)) || 
               ((coords[0] == dims[0]-1) && (ii >= DIM1+H1)) ) {
            tax=triple_t<USE_DOUBLE>().x();
            tbx=triple_t<USE_DOUBLE>().x();
            tcx=triple_t<USE_DOUBLE>().x();
          }
        }

        if (!per1) {
          if ( ((coords[1]==0) && (jj<H2)) || 
               ((coords[1] == dims[1]-1) && (jj >= DIM2+H2)) ) {
            tay=triple_t<USE_DOUBLE>().y();
            tby=triple_t<USE_DOUBLE>().y();
            tcy=triple_t<USE_DOUBLE>().y();
          }
        }

        if (!per2) {
          if ( ((coords[2]==0) && (kk<H3)) || 
               ((coords[2] == dims[2]-1) && (kk >= DIM3+H3)) ) {
            taz=triple_t<USE_DOUBLE>().z();
            tbz=triple_t<USE_DOUBLE>().z();
            tcz=triple_t<USE_DOUBLE>().z();
          }
        }

        ta = triple_t<USE_DOUBLE>(tax, tay, taz).floor();
        tb = triple_t<USE_DOUBLE>(tbx, tby, tbz).floor();
        tc = triple_t<USE_DOUBLE>(tcx, tcy, tcz).floor();

        if (a(ii,jj,kk) != ta) {
          passed = false;
          file << ii << ", " << jj << ", " << kk << " values found != expct: " 
               << "a " << a(ii,jj,kk) << " != " 
               << ta
               << "\n";
        }

        if (b(ii,jj,kk) != tb) {
          passed = false;
          file << ii << ", " << jj << ", " << kk << " values found != expct: " 
               << "b " << b(ii,jj,kk) << " != " 
               << tb
               << "\n";
        }

        if (c(ii,jj,kk) != tc) {
          passed = false;
          file << ii << ", " << jj << ", " << kk << " values found != expct: " 
               << "c " << c(ii,jj,kk) << " != " 
               << tc
               << "\n";
        }
      }

  if (passed)
    file << "RESULT: PASSED!\n";
  else
    file << "RESULT: FAILED!\n";

}

int main(int argc, char** argv) {


  /* this example is based on MPI Cart Communicators, so we need to
  initialize MPI. This can be done by GCL automatically
  */
  MPI_Init(&argc, &argv);


  /* Now let us initialize GCL itself. If MPI is not initialized at
     this point, it will initialize it
   */
  gridtools::GCL_Init(argc, argv);

  /* Here we compute the computing gris as in many applications
   */
  MPI_Comm_rank(MPI_COMM_WORLD, &pid);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);



  std::cout << pid << " " << nprocs << "\n";

  std::stringstream ss;
  ss << pid;

  std::string filename = "out" + ss.str() + ".txt";

  std::cout << filename << std::endl;
  std::ofstream file(filename.c_str());

  file << pid << "  " << nprocs << "\n";

  MPI_Dims_create(nprocs, 3, dims);
  int period[3] = {1, 1, 1};

  file << "@" << pid << "@ MPI GRID SIZE " << dims[0] << " - " << dims[1] << " - " << dims[2] << "\n";
 
  MPI_Cart_create(MPI_COMM_WORLD, 3, dims, period, false, &CartComm);

  MPI_Cart_get(CartComm, 3, dims, period, coords);


  /* Each process will hold a tile of size
     (DIM1+2*H)x(DIM2+2*H)x(DIM3+2*H). The DIM1xDIM2xDIM3 area inside
     the H width border is the inner region of an hypothetical stencil
     computation whise halo width is H.
   */
  int DIM1=atoi(argv[1]);
  int DIM2=atoi(argv[2]);
  int DIM3=atoi(argv[3]);
  int H1  =atoi(argv[4]);
  int H2  =atoi(argv[5]);
  int H3  =atoi(argv[6]);


  /* This example will exchange 3 data arrays at the same time with
     different values.
   */
  triple_t<USE_DOUBLE> *_a = new triple_t<USE_DOUBLE>[(DIM1+2*H1)*(DIM2+2*H2)*(DIM3+2*H3)];
  triple_t<USE_DOUBLE> *_b = new triple_t<USE_DOUBLE>[(DIM1+2*H1)*(DIM2+2*H2)*(DIM3+2*H3)];
  triple_t<USE_DOUBLE> *_c = new triple_t<USE_DOUBLE>[(DIM1+2*H1)*(DIM2+2*H2)*(DIM3+2*H3)];


#ifdef GCL_TRACE
  gridtools::stats_collector_3D.recording(true);
#endif
#ifdef BENCH
  for (int i=0; i<BENCH; ++i) {
    file << "run<std::ostream, 0,1,2, true, true, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
    run<std::ostream, 0,1,2, true, true, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);
  }
#else
  file << "Permutation 0,1,2\n";

  file << "run<std::ostream, 0,1,2, true, true, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 0,1,2, true, true, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 0,1,2, true, true, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 0,1,2, true, true, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 0,1,2, true, false, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 0,1,2, true, false, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 0,1,2, true, false, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 0,1,2, true, false, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 0,1,2, false, true, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 0,1,2, false, true, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 0,1,2, false, true, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 0,1,2, false, true, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 0,1,2, false, false, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 0,1,2, false, false, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 0,1,2, false, false, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 0,1,2, false, false, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);
  file << "---------------------------------------------------\n";


  file << "Permutation 0,2,1\n";

  file << "run<std::ostream, 0,2,1, true, true, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 0,2,1, true, true, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 0,2,1, true, true, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 0,2,1, true, true, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 0,2,1, true, false, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 0,2,1, true, false, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 0,2,1, true, false, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 0,2,1, true, false, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 0,2,1, false, true, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 0,2,1, false, true, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 0,2,1, false, true, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 0,2,1, false, true, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 0,2,1, false, false, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 0,2,1, false, false, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 0,2,1, false, false, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 0,2,1, false, false, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);
  file << "---------------------------------------------------\n";


  file << "Permutation 1,0,2\n";

  file << "run<std::ostream, 1,0,2, true, true, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 1,0,2, true, true, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 1,0,2, true, true, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 1,0,2, true, true, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 1,0,2, true, false, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 1,0,2, true, false, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 1,0,2, true, false, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 1,0,2, true, false, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 1,0,2, false, true, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 1,0,2, false, true, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 1,0,2, false, true, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 1,0,2, false, true, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 1,0,2, false, false, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 1,0,2, false, false, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 1,0,2, false, false, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 1,0,2, false, false, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);
  file << "---------------------------------------------------\n";


  file << "Permutation 1,2,0\n";

  file << "run<std::ostream, 1,2,0, true, true, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 1,2,0, true, true, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 1,2,0, true, true, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 1,2,0, true, true, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 1,2,0, true, false, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 1,2,0, true, false, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 1,2,0, true, false, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 1,2,0, true, false, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 1,2,0, false, true, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 1,2,0, false, true, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 1,2,0, false, true, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 1,2,0, false, true, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 1,2,0, false, false, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 1,2,0, false, false, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 1,2,0, false, false, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 1,2,0, false, false, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);
  file << "---------------------------------------------------\n";


  file << "Permutation 2,0,1\n";

  file << "run<std::ostream, 2,0,1, true, true, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 2,0,1, true, true, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 2,0,1, true, true, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 2,0,1, true, true, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 2,0,1, true, false, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 2,0,1, true, false, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 2,0,1, true, false, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 2,0,1, true, false, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 2,0,1, false, true, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 2,0,1, false, true, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 2,0,1, false, true, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 2,0,1, false, true, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 2,0,1, false, false, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 2,0,1, false, false, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 2,0,1, false, false, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 2,0,1, false, false, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);
  file << "---------------------------------------------------\n";


  file << "Permutation 2,1,0\n";

  file << "run<std::ostream, 2,1,0, true, true, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 2,1,0, true, true, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 2,1,0, true, true, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 2,1,0, true, true, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 2,1,0, true, false, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 2,1,0, true, false, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 2,1,0, true, false, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 2,1,0, true, false, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 2,1,0, false, true, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 2,1,0, false, true, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 2,1,0, false, true, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 2,1,0, false, true, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 2,1,0, false, false, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 2,1,0, false, false, true>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);

  file << "run<std::ostream, 2,1,0, false, false, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c)\n";
  run<std::ostream, 2,1,0, false, false, false>(file, DIM1, DIM2, DIM3, H1, H2, H3, _a, _b, _c);
  file << "---------------------------------------------------\n";
#endif

#ifdef GCL_TRACE
  gridtools::stats_collector_3D.evaluate(std::cout);
#endif

  delete[] _a;
  delete[] _b;
  delete[] _c;

  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Finalize();
  return 0;
}